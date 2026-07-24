use axum::extract::{Path, State};
use axum::Json;
use serde::{Deserialize, Serialize};

use crate::error::ApiError;
use crate::nodeid::parse_node_id;
use crate::state::AppState;

#[derive(Deserialize)]
pub struct OneTimePrekeyIn {
    pub prekey_id: i32,
    pub prekey_pub: String, // hex
}

#[derive(Deserialize)]
pub struct PublishBundleReq {
    pub identity_pub: String, // hex
    pub registration_id: i32,
    pub signed_prekey_id: i32,
    pub signed_prekey_pub: String, // hex
    pub signed_prekey_sig: String, // hex
    pub one_time_prekeys: Vec<OneTimePrekeyIn>,
}

// PUT /v1/keys/:node_id - wezel publikuje/odswiez swoj komplet kluczy. To odpowiednik
// tego co mesh.c robi bezposrednio miedzy sasiadami (PKT_TYPE_KEY_BUNDLE), tylko przez
// serwer, wiec dziala nawet gdy odbiorca akurat nie jest w bezposrednim zasiegu.
pub async fn publish_bundle(
    State(state): State<AppState>,
    Path(node_id_hex): Path<String>,
    Json(req): Json<PublishBundleReq>,
) -> Result<(), ApiError> {
    let node_id = parse_node_id(&node_id_hex)?;
    let identity_pub = hex::decode(&req.identity_pub)
        .map_err(|_| ApiError::BadRequest("zly hex w identity_pub".into()))?;
    let signed_prekey_pub = hex::decode(&req.signed_prekey_pub)
        .map_err(|_| ApiError::BadRequest("zly hex w signed_prekey_pub".into()))?;
    let signed_prekey_sig = hex::decode(&req.signed_prekey_sig)
        .map_err(|_| ApiError::BadRequest("zly hex w signed_prekey_sig".into()))?;

    sqlx::query(
        "insert into nodes (node_id, identity_pub, registration_id, signed_prekey_id, signed_prekey_pub, signed_prekey_sig, updated_at)
         values ($1, $2, $3, $4, $5, $6, now())
         on conflict (node_id) do update set
            identity_pub = excluded.identity_pub,
            registration_id = excluded.registration_id,
            signed_prekey_id = excluded.signed_prekey_id,
            signed_prekey_pub = excluded.signed_prekey_pub,
            signed_prekey_sig = excluded.signed_prekey_sig,
            updated_at = now()",
    )
    .bind(&node_id)
    .bind(&identity_pub)
    .bind(req.registration_id)
    .bind(req.signed_prekey_id)
    .bind(&signed_prekey_pub)
    .bind(&signed_prekey_sig)
    .execute(&state.db)
    .await?;

    // one-time prekeys dokladamy, nie zastepujemy - klient sam wie ktore juz wyslal
    // wczesniej (ON CONFLICT DO NOTHING po (node_id, prekey_id))
    for pk in req.one_time_prekeys {
        let prekey_pub = hex::decode(&pk.prekey_pub)
            .map_err(|_| ApiError::BadRequest("zly hex w prekey_pub".into()))?;
        sqlx::query(
            "insert into one_time_prekeys (node_id, prekey_id, prekey_pub)
             values ($1, $2, $3)
             on conflict (node_id, prekey_id) do nothing",
        )
        .bind(&node_id)
        .bind(pk.prekey_id)
        .bind(&prekey_pub)
        .execute(&state.db)
        .await?;
    }

    Ok(())
}

#[derive(Serialize)]
pub struct BundleResp {
    pub identity_pub: String,
    pub registration_id: i32,
    pub signed_prekey_id: i32,
    pub signed_prekey_pub: String,
    pub signed_prekey_sig: String,
    // brak = pula one-time prekeys pusta. X3DH dziala tez bez niego (troche slabszy
    // forward secrecy na pierwszej wiadomosci) - firmware/components/crypto/pcrypto.c
    // (wspolny z client/desktop) to teraz poprawnie obsluguje po obu stronach
    pub one_time_prekey: Option<OneTimePrekeyOut>,
}

#[derive(Serialize)]
pub struct OneTimePrekeyOut {
    pub prekey_id: i32,
    pub prekey_pub: String,
}

// GET /v1/keys/:node_id - ktos chce zaczac sesje z node_id, pobiera jego bundle.
// Zuzywa jeden one-time prekey (oznacza go jako uzyty), zeby nie dalo sie go zuzyc dwa razy.
pub async fn get_bundle(
    State(state): State<AppState>,
    Path(node_id_hex): Path<String>,
) -> Result<Json<BundleResp>, ApiError> {
    let node_id = parse_node_id(&node_id_hex)?;

    let node = sqlx::query_as::<_, (Vec<u8>, i32, i32, Vec<u8>, Vec<u8>)>(
        "select identity_pub, registration_id, signed_prekey_id, signed_prekey_pub, signed_prekey_sig
         from nodes where node_id = $1",
    )
    .bind(&node_id)
    .fetch_optional(&state.db)
    .await?
    .ok_or(ApiError::NotFound)?;

    let one_time = sqlx::query_as::<_, (i32, Vec<u8>)>(
        "update one_time_prekeys set used_at = now()
         where id = (
             select id from one_time_prekeys
             where node_id = $1 and used_at is null
             order by id
             limit 1
             for update skip locked
         )
         returning prekey_id, prekey_pub",
    )
    .bind(&node_id)
    .fetch_optional(&state.db)
    .await?;

    Ok(Json(BundleResp {
        identity_pub: hex::encode(node.0),
        registration_id: node.1,
        signed_prekey_id: node.2,
        signed_prekey_pub: hex::encode(node.3),
        signed_prekey_sig: hex::encode(node.4),
        one_time_prekey: one_time.map(|(id, pub_key)| OneTimePrekeyOut {
            prekey_id: id,
            prekey_pub: hex::encode(pub_key),
        }),
    }))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::state::test_state;
    use sqlx::PgPool;

    fn bundle_req(one_time_prekeys: Vec<OneTimePrekeyIn>) -> PublishBundleReq {
        PublishBundleReq {
            identity_pub: "aa".repeat(32),
            registration_id: 1,
            signed_prekey_id: 1,
            signed_prekey_pub: "bb".repeat(32),
            signed_prekey_sig: "cc".repeat(64),
            one_time_prekeys,
        }
    }

    #[sqlx::test]
    async fn publish_then_get_bundle_roundtrip(pool: PgPool) {
        let state = test_state(pool).await;
        publish_bundle(
            State(state.clone()),
            Path("a4cf12b8aabb0000".into()),
            Json(bundle_req(vec![])),
        )
        .await
        .unwrap();

        let resp = get_bundle(State(state), Path("a4cf12b8aabb0000".into()))
            .await
            .unwrap();
        assert_eq!(resp.0.identity_pub, "aa".repeat(32));
        assert_eq!(resp.0.registration_id, 1);
        assert!(resp.0.one_time_prekey.is_none());
    }

    #[sqlx::test]
    async fn get_bundle_for_unknown_node_is_not_found(pool: PgPool) {
        let state = test_state(pool).await;
        let result = get_bundle(State(state), Path("0000000000000000".into())).await;
        assert!(matches!(result, Err(ApiError::NotFound)));
    }

    #[sqlx::test]
    async fn one_time_prekey_is_consumed_exactly_once(pool: PgPool) {
        let state = test_state(pool).await;
        publish_bundle(
            State(state.clone()),
            Path("a4cf12b8aabb0000".into()),
            Json(bundle_req(vec![OneTimePrekeyIn {
                prekey_id: 7,
                prekey_pub: "dd".repeat(32),
            }])),
        )
        .await
        .unwrap();

        let first = get_bundle(State(state.clone()), Path("a4cf12b8aabb0000".into()))
            .await
            .unwrap();
        assert!(first.0.one_time_prekey.is_some());

        let second = get_bundle(State(state), Path("a4cf12b8aabb0000".into()))
            .await
            .unwrap();
        assert!(second.0.one_time_prekey.is_none());
    }
}
