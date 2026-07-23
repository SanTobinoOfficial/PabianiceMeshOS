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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_valid_node_id() {
        let bytes = parse_node_id("a4cf12b8aabb0000").unwrap();
        assert_eq!(bytes, vec![0xa4, 0xcf, 0x12, 0xb8, 0xaa, 0xbb, 0x00, 0x00]);
    }

    #[test]
    fn rejects_non_hex_input() {
        assert!(matches!(
            parse_node_id("nie-hex-string!!"),
            Err(ApiError::BadRequest(_))
        ));
    }

    #[test]
    fn rejects_wrong_length() {
        assert!(matches!(
            parse_node_id("aabb"),
            Err(ApiError::BadRequest(_))
        ));
        assert!(matches!(
            parse_node_id("a4cf12b8aabb000011"),
            Err(ApiError::BadRequest(_))
        ));
    }
}
