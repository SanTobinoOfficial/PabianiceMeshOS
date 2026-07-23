use crate::error::ApiError;

// node_id w firmware to 8 surowych bajtow (PKT_NODE_ID_LEN, pkt.h) - w URL-ach i JSON-ie
// nosimy je jako hex, 16 znakow.
pub const NODE_ID_LEN: usize = 8;

pub fn parse_node_id(hex_str: &str) -> Result<Vec<u8>, ApiError> {
    let bytes =
        hex::decode(hex_str).map_err(|_| ApiError::BadRequest("zly hex w node_id".into()))?;
    if bytes.len() != NODE_ID_LEN {
        return Err(ApiError::BadRequest(format!(
            "node_id musi miec {NODE_ID_LEN} bajtow, dostalem {}",
            bytes.len()
        )));
    }
    Ok(bytes)
}
