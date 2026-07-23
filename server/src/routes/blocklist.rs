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
