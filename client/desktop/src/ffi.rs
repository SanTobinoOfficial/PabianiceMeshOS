// Surowe deklaracje extern "C" - lustro firmware/components/crypto/include/pcrypto.h.
// Nie duplikujemy logiki, tylko wolamy dokladnie te same funkcje C skompilowane przez
// build.rs. Zobacz src/pcrypto.rs po bezpieczny wrapper.

#![allow(dead_code)]

use std::os::raw::{c_char, c_int};

pub const PCRYPTO_PEER_ID_LEN: usize = 8;
pub const PCRYPTO_BUNDLE_MAX_LEN: usize = 199;

#[repr(C)]
pub struct pcrypto_bundle_t {
    pub data: [u8; PCRYPTO_BUNDLE_MAX_LEN],
    pub len: usize,
}

impl Default for pcrypto_bundle_t {
    fn default() -> Self {
        pcrypto_bundle_t {
            data: [0u8; PCRYPTO_BUNDLE_MAX_LEN],
            len: 0,
        }
    }
}

pub const ESP_OK: c_int = 0;

extern "C" {
    pub fn host_shim_set_data_dir(dir: *const c_char);

    pub fn pcrypto_init() -> c_int;
    pub fn pcrypto_local_bundle(out: *mut pcrypto_bundle_t) -> c_int;
    pub fn pcrypto_process_bundle(
        peer_id: *const u8,
        bundle_data: *const u8,
        bundle_len: usize,
    ) -> c_int;
    pub fn pcrypto_has_session(peer_id: *const u8) -> bool;
    pub fn pcrypto_encrypt(
        peer_id: *const u8,
        plaintext: *const u8,
        plaintext_len: usize,
        out: *mut u8,
        out_cap: usize,
        out_len: *mut usize,
    ) -> c_int;
    pub fn pcrypto_decrypt(
        peer_id: *const u8,
        ciphertext: *const u8,
        ciphertext_len: usize,
        out: *mut u8,
        out_cap: usize,
        out_len: *mut usize,
    ) -> c_int;
}
