// Klient HTTP do /server - te same 4 endpointy ktorych uzywalby bramka mesh<->internet
// (server/src/routes/keys.rs, messages.rs). Serwer widzi tylko hexy (ciphertext, klucze
// publiczne) - nigdy plaintextu ani prywatnych kluczy, dokladnie tak jak firmware.

use anyhow::{bail, Result};
use serde::{Deserialize, Serialize};

use crate::pcrypto::BundleFields;

pub struct Api {
    base_url: String,
    client: reqwest::blocking::Client,
}

#[derive(Serialize)]
struct OneTimePrekeyIn {
    prekey_id: i32,
    prekey_pub: String,
}

#[derive(Serialize)]
struct PublishBundleReq {
    identity_pub: String,
    registration_id: i32,
    signed_prekey_id: i32,
    signed_prekey_pub: String,
    signed_prekey_sig: String,
    one_time_prekeys: Vec<OneTimePrekeyIn>,
}

#[derive(Deserialize)]
struct OneTimePrekeyOut {
    prekey_id: i32,
    prekey_pub: String,
}

#[derive(Deserialize)]
struct BundleResp {
    identity_pub: String,
    registration_id: i32,
    signed_prekey_id: i32,
    signed_prekey_pub: String,
    signed_prekey_sig: String,
    one_time_prekey: Option<OneTimePrekeyOut>,
}

#[derive(Serialize)]
struct SubmitMessageReq {
    dst_id: String,
    src_id: String,
    ciphertext: String,
}

#[derive(Deserialize)]
pub struct SubmitMessageResp {
    pub id: String,
}

#[derive(Deserialize, Debug)]
pub struct PendingMessage {
    pub id: String,
    pub src_id: String,
    pub ciphertext: String,
    pub created_at: String,
}

impl Api {
    pub fn new(base_url: String) -> Self {
        Api {
            base_url: base_url.trim_end_matches('/').to_string(),
            client: reqwest::blocking::Client::new(),
        }
    }

    pub fn publish_bundle(&self, node_id_hex: &str, fields: &BundleFields) -> Result<()> {
        let req = PublishBundleReq {
            identity_pub: hex::encode(&fields.identity_pub),
            registration_id: fields.registration_id as i32,
            signed_prekey_id: fields.signed_pre_key_id as i32,
            signed_prekey_pub: hex::encode(&fields.signed_pre_key_pub),
            signed_prekey_sig: hex::encode(&fields.signed_pre_key_sig),
            one_time_prekeys: vec![OneTimePrekeyIn {
                prekey_id: fields.pre_key_id as i32,
                prekey_pub: hex::encode(&fields.pre_key_pub),
            }],
        };

        let resp = self
            .client
            .put(format!("{}/v1/keys/{}", self.base_url, node_id_hex))
            .json(&req)
            .send()?;
        if !resp.status().is_success() {
            bail!(
                "PUT /v1/keys/{node_id_hex} -> {} {}",
                resp.status(),
                resp.text().unwrap_or_default()
            );
        }
        Ok(())
    }

    pub fn fetch_bundle(&self, node_id_hex: &str) -> Result<BundleFields> {
        let resp = self
            .client
            .get(format!("{}/v1/keys/{}", self.base_url, node_id_hex))
            .send()?;
        if !resp.status().is_success() {
            bail!(
                "GET /v1/keys/{node_id_hex} -> {} {}",
                resp.status(),
                resp.text().unwrap_or_default()
            );
        }
        let body: BundleResp = resp.json()?;
        // Brak opublikowanego one-time prekey (serwer juz rozdal jedyny albo wezel
        // jeszcze zadnego nie opublikowal) nie jest bledem - X3DH dziala tez bez OPK,
        // kosztem slabszego forward secrecy pierwszej wiadomosci (patrz pcrypto.c,
        // ktory teraz to samo toleruje po stronie parsera bundla). Puste pole = brak.
        let (pre_key_id, pre_key_pub) = match body.one_time_prekey {
            Some(otp) => (otp.prekey_id as u32, hex::decode(otp.prekey_pub)?),
            None => (0, Vec::new()),
        };

        Ok(BundleFields {
            registration_id: body.registration_id as u32,
            pre_key_id,
            pre_key_pub,
            signed_pre_key_id: body.signed_prekey_id as u32,
            signed_pre_key_pub: hex::decode(body.signed_prekey_pub)?,
            signed_pre_key_sig: hex::decode(body.signed_prekey_sig)?,
            identity_pub: hex::decode(body.identity_pub)?,
        })
    }

    pub fn submit_message(
        &self,
        dst_id_hex: &str,
        src_id_hex: &str,
        ciphertext: &[u8],
    ) -> Result<String> {
        let req = SubmitMessageReq {
            dst_id: dst_id_hex.to_string(),
            src_id: src_id_hex.to_string(),
            ciphertext: hex::encode(ciphertext),
        };
        let resp = self
            .client
            .post(format!("{}/v1/messages", self.base_url))
            .json(&req)
            .send()?;
        if !resp.status().is_success() {
            bail!(
                "POST /v1/messages -> {} {}",
                resp.status(),
                resp.text().unwrap_or_default()
            );
        }
        let body: SubmitMessageResp = resp.json()?;
        Ok(body.id)
    }

    pub fn poll_messages(&self, node_id_hex: &str) -> Result<Vec<PendingMessage>> {
        let resp = self
            .client
            .get(format!("{}/v1/messages/{}", self.base_url, node_id_hex))
            .send()?;
        if !resp.status().is_success() {
            bail!(
                "GET /v1/messages/{node_id_hex} -> {} {}",
                resp.status(),
                resp.text().unwrap_or_default()
            );
        }
        Ok(resp.json()?)
    }

    pub fn ack_message(&self, node_id_hex: &str, message_id: &str) -> Result<()> {
        let resp = self
            .client
            .delete(format!(
                "{}/v1/messages/{}/id/{}",
                self.base_url, node_id_hex, message_id
            ))
            .send()?;
        if !resp.status().is_success() {
            bail!(
                "DELETE /v1/messages/{node_id_hex}/id/{message_id} -> {}",
                resp.status()
            );
        }
        Ok(())
    }
}
