// Bezpieczny wrapper nad ffi.rs + kodek wlasnego formatu bundla z pcrypto.c (patrz
// bundle_put_u32/bundle_put_blob tamze - 4 bajty u32 (native endian, tak samo jak
// memcpy w oryginale), potem blob jako 1 bajt dlugosci + dane, max 255B na pole).
// Serwer (/server) przechowuje te same pola jako osobne hex-owane stringi w JSON, wiec
// przy publikacji dekodujemy bundle na pola, a przy odbiorze bundla peera koduje sie
// pola z powrotem na ten sam binarny format, zeby wrzucic je w pcrypto_process_bundle
// bez zmiany jednej linijki C.

use crate::ffi::{self, pcrypto_bundle_t, ESP_OK};
use anyhow::{anyhow, bail, Result};
use std::ffi::CString;
use std::path::Path;

pub const PEER_ID_LEN: usize = ffi::PCRYPTO_PEER_ID_LEN;

const OUT_BUF_CAP: usize = 8192;

pub struct LocalIdentity {
    _private: (), // pcrypto.c trzyma caly stan w statykach C - jeden proces = jedna tozsamosc
}

#[derive(Debug, Clone)]
pub struct BundleFields {
    pub registration_id: u32,
    pub pre_key_id: u32,
    pub pre_key_pub: Vec<u8>,
    pub signed_pre_key_id: u32,
    pub signed_pre_key_pub: Vec<u8>,
    pub signed_pre_key_sig: Vec<u8>,
    pub identity_pub: Vec<u8>,
}

impl LocalIdentity {
    /// Wywoluje host_shim_set_data_dir + pcrypto_init. Wolne raz na proces - druga
    /// tozsamosc w tym samym procesie nie jest wspierana (statyki w pcrypto.c).
    pub fn init(data_dir: &Path) -> Result<Self> {
        std::fs::create_dir_all(data_dir)?;
        let dir_str = data_dir
            .to_str()
            .ok_or_else(|| anyhow!("sciezka data-dir zawiera nie-UTF8 znaki"))?;
        let dir_c = CString::new(dir_str)?;
        unsafe {
            ffi::host_shim_set_data_dir(dir_c.as_ptr());
            let rc = ffi::pcrypto_init();
            if rc != ESP_OK {
                bail!("pcrypto_init nie wyszlo (esp_err_t={rc})");
            }
        }
        Ok(LocalIdentity { _private: () })
    }

    pub fn local_bundle(&self) -> Result<BundleFields> {
        let mut raw = pcrypto_bundle_t::default();
        let rc = unsafe { ffi::pcrypto_local_bundle(&mut raw) };
        if rc != ESP_OK {
            bail!(
                "pcrypto_local_bundle nie wyszlo (esp_err_t={rc}) - brak wolnych one-time prekeys?"
            );
        }
        decode_bundle(&raw.data[..raw.len])
    }

    pub fn process_peer_bundle(
        &self,
        peer_id: &[u8; PEER_ID_LEN],
        fields: &BundleFields,
    ) -> Result<()> {
        let encoded = encode_bundle(fields)?;
        let rc = unsafe {
            ffi::pcrypto_process_bundle(peer_id.as_ptr(), encoded.as_ptr(), encoded.len())
        };
        if rc != ESP_OK {
            bail!("pcrypto_process_bundle nie wyszlo (esp_err_t={rc}) - zly bundle albo untrusted identity");
        }
        Ok(())
    }

    pub fn has_session(&self, peer_id: &[u8; PEER_ID_LEN]) -> bool {
        unsafe { ffi::pcrypto_has_session(peer_id.as_ptr()) }
    }

    pub fn encrypt(&self, peer_id: &[u8; PEER_ID_LEN], plaintext: &[u8]) -> Result<Vec<u8>> {
        let mut out = vec![0u8; OUT_BUF_CAP];
        let mut out_len: usize = 0;
        let rc = unsafe {
            ffi::pcrypto_encrypt(
                peer_id.as_ptr(),
                plaintext.as_ptr(),
                plaintext.len(),
                out.as_mut_ptr(),
                out.len(),
                &mut out_len,
            )
        };
        if rc != ESP_OK {
            bail!("pcrypto_encrypt nie wyszlo (esp_err_t={rc}) - brak sesji z peerem?");
        }
        out.truncate(out_len);
        Ok(out)
    }

    pub fn decrypt(&self, peer_id: &[u8; PEER_ID_LEN], ciphertext: &[u8]) -> Result<Vec<u8>> {
        let mut out = vec![0u8; OUT_BUF_CAP];
        let mut out_len: usize = 0;
        let rc = unsafe {
            ffi::pcrypto_decrypt(
                peer_id.as_ptr(),
                ciphertext.as_ptr(),
                ciphertext.len(),
                out.as_mut_ptr(),
                out.len(),
                &mut out_len,
            )
        };
        if rc != ESP_OK {
            bail!("pcrypto_decrypt nie wyszlo (esp_err_t={rc})");
        }
        out.truncate(out_len);
        Ok(out)
    }
}

fn put_u32(buf: &mut Vec<u8>, v: u32) {
    buf.extend_from_slice(&v.to_ne_bytes());
}

fn put_blob(buf: &mut Vec<u8>, data: &[u8]) -> Result<()> {
    if data.len() > 255 {
        bail!(
            "pole bundla dluzsze niz 255B ({}), format pcrypto.c tego nie udzwignie",
            data.len()
        );
    }
    buf.push(data.len() as u8);
    buf.extend_from_slice(data);
    Ok(())
}

fn encode_bundle(f: &BundleFields) -> Result<Vec<u8>> {
    let mut buf = Vec::with_capacity(ffi::PCRYPTO_BUNDLE_MAX_LEN);
    put_u32(&mut buf, f.registration_id);
    put_u32(&mut buf, f.pre_key_id);
    put_blob(&mut buf, &f.pre_key_pub)?;
    put_u32(&mut buf, f.signed_pre_key_id);
    put_blob(&mut buf, &f.signed_pre_key_pub)?;
    put_blob(&mut buf, &f.signed_pre_key_sig)?;
    put_blob(&mut buf, &f.identity_pub)?;
    Ok(buf)
}

fn need(data: &[u8], off: usize, n: usize) -> Result<()> {
    if off + n > data.len() {
        bail!("bundle za krotki - ucieta transmisja albo zly format");
    }
    Ok(())
}

fn get_u32(data: &[u8], off: &mut usize) -> Result<u32> {
    need(data, *off, 4)?;
    let v = u32::from_ne_bytes(data[*off..*off + 4].try_into().unwrap());
    *off += 4;
    Ok(v)
}

fn get_blob(data: &[u8], off: &mut usize) -> Result<Vec<u8>> {
    need(data, *off, 1)?;
    let len = data[*off] as usize;
    *off += 1;
    need(data, *off, len)?;
    let v = data[*off..*off + len].to_vec();
    *off += len;
    Ok(v)
}

fn decode_bundle(data: &[u8]) -> Result<BundleFields> {
    let mut off = 0usize;
    let registration_id = get_u32(data, &mut off)?;
    let pre_key_id = get_u32(data, &mut off)?;
    let pre_key_pub = get_blob(data, &mut off)?;
    let signed_pre_key_id = get_u32(data, &mut off)?;
    let signed_pre_key_pub = get_blob(data, &mut off)?;
    let signed_pre_key_sig = get_blob(data, &mut off)?;
    let identity_pub = get_blob(data, &mut off)?;
    Ok(BundleFields {
        registration_id,
        pre_key_id,
        pre_key_pub,
        signed_pre_key_id,
        signed_pre_key_pub,
        signed_pre_key_sig,
        identity_pub,
    })
}
