use axum::extract::{Path, State};
use axum::Json;
use serde::{Deserialize, Serialize};

use crate::error::ApiError;
use crate::nodeid::parse_node_id;
use crate::state::AppState;

// Blokowanie po HWID, tylko w tym trybie scentralizowanym (rozdz. 7.3.1 planu) - w
// trybie czysto lokalnym P2P nie ma jak tego bezpiecznie zrobic, patrz 7.3.2.
//
// Wymaga bearer tokena administratora (routes/mod.rs, auth.rs) - pelny panel webowy
// z kontami i 2FA to dopiero Pabianice OS (rozdz. 10.4 planu), swiadomie pozniejszy etap.

#[derive(Deserialize)]
pub struct BlockReq {
    pub reason: Option<String>,
}

pub async fn block_device(
    State(state): State<AppState>,
    Path(hwid_hex): Path<String>,
    Json(req): Json<BlockReq>,
) -> Result<(), ApiError> {
    let hwid = parse_node_id(&hwid_hex)?;
    sqlx::query(
        "insert into blocked_devices (hwid, reason) values ($1, $2)
         on conflict (hwid) do update set reason = excluded.reason",
    )
    .bind(&hwid)
    .bind(req.reason)
    .execute(&state.db)
    .await?;
    Ok(())
}

pub async fn unblock_device(
    State(state): State<AppState>,
    Path(hwid_hex): Path<String>,
) -> Result<(), ApiError> {
    let hwid = parse_node_id(&hwid_hex)?;
    sqlx::query("delete from blocked_devices where hwid = $1")
        .bind(&hwid)
        .execute(&state.db)
        .await?;
    Ok(())
}

#[derive(Serialize)]
pub struct BlockedEntry {
    pub hwid: String,
    pub reason: Option<String>,
}

pub async fn list_blocked(
    State(state): State<AppState>,
) -> Result<Json<Vec<BlockedEntry>>, ApiError> {
    let rows = sqlx::query_as::<_, (Vec<u8>, Option<String>)>(
        "select hwid, reason from blocked_devices order by blocked_at desc",
    )
    .fetch_all(&state.db)
    .await?;

    Ok(Json(
        rows.into_iter()
            .map(|(hwid, reason)| BlockedEntry {
                hwid: hex::encode(hwid),
                reason,
            })
            .collect(),
    ))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::state::test_state;
    use sqlx::PgPool;

    #[sqlx::test]
    async fn block_then_list_then_unblock(pool: PgPool) {
        let state = test_state(pool).await;
        let hwid = "aabbccdd11223344";

        block_device(
            State(state.clone()),
            Path(hwid.into()),
            Json(BlockReq {
                reason: Some("test".into()),
            }),
        )
        .await
        .unwrap();

        let listed = list_blocked(State(state.clone())).await.unwrap();
        assert_eq!(listed.0.len(), 1);
        assert_eq!(listed.0[0].hwid, hwid);
        assert_eq!(listed.0[0].reason.as_deref(), Some("test"));

        unblock_device(State(state.clone()), Path(hwid.into()))
            .await
            .unwrap();

        let listed_after = list_blocked(State(state)).await.unwrap();
        assert_eq!(listed_after.0.len(), 0);
    }

    #[sqlx::test]
    async fn blocking_twice_updates_reason_instead_of_duplicating(pool: PgPool) {
        let state = test_state(pool).await;
        let hwid = "aabbccdd11223344";

        block_device(
            State(state.clone()),
            Path(hwid.into()),
            Json(BlockReq { reason: None }),
        )
        .await
        .unwrap();
        block_device(
            State(state.clone()),
            Path(hwid.into()),
            Json(BlockReq {
                reason: Some("drugi powod".into()),
            }),
        )
        .await
        .unwrap();

        let listed = list_blocked(State(state)).await.unwrap();
        assert_eq!(listed.0.len(), 1);
        assert_eq!(listed.0[0].reason.as_deref(), Some("drugi powod"));
    }
}
