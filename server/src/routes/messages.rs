use axum::extract::{Path, State};
use axum::Json;
use chrono::{DateTime, Duration, Utc};
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::error::ApiError;
use crate::nodeid::parse_node_id;
use crate::state::AppState;

#[derive(Deserialize)]
pub struct SubmitMessageReq {
    pub dst_id: String,     // hex
    pub src_id: String,     // hex
    pub ciphertext: String, // hex - to co juz wyszlo z pcrypto_encrypt po stronie klienta
}

#[derive(Serialize)]
pub struct SubmitMessageResp {
    pub id: Uuid,
}

// POST /v1/messages - wezel z dostepem do internetu wypycha wiadomosc, ktorej nie
// udalo mu sie dostarczyc bezposrednio przez mesh w ciagu ~24h (rozdz. 4.2 krok 6 planu).
// Serwer widzi tylko ciphertext, dst_id/src_id (hashe kluczy) i tyle.
pub async fn submit_message(
    State(state): State<AppState>,
    Json(req): Json<SubmitMessageReq>,
) -> Result<Json<SubmitMessageResp>, ApiError> {
    let dst_id = parse_node_id(&req.dst_id)?;
    let src_id = parse_node_id(&req.src_id)?;
    let ciphertext = hex::decode(&req.ciphertext)
        .map_err(|_| ApiError::BadRequest("zly hex w ciphertext".into()))?;

    if is_blocked(&state, &src_id).await? {
        return Err(ApiError::Blocked);
    }

    let expires_at: DateTime<Utc> = Utc::now() + Duration::days(state.retention_days);

    let id: Uuid = sqlx::query_scalar(
        "insert into messages (dst_id, src_id, ciphertext, expires_at)
         values ($1, $2, $3, $4)
         returning id",
    )
    .bind(&dst_id)
    .bind(&src_id)
    .bind(&ciphertext)
    .bind(expires_at)
    .fetch_one(&state.db)
    .await?;

    // nudge przez Redis - jesli jakis gateway trzyma otwarte polaczenie/subskrypcje
    // dla tego node_id, moze odebrac od razu zamiast czekac na kolejny poll. Jesli
    // Redis akurat nie dziala, i tak nic sie nie traci - polling GET /v1/messages
    // zawsze znajdzie wiadomosc w Postgresie, to tylko przyspieszenie
    let mut redis_conn = state.redis.clone();
    let channel = format!("notify:{}", req.dst_id);
    let _: Result<(), _> = redis::cmd("PUBLISH")
        .arg(&channel)
        .arg("1")
        .query_async(&mut redis_conn)
        .await;

    Ok(Json(SubmitMessageResp { id }))
}

#[derive(Serialize)]
pub struct PendingMessage {
    pub id: Uuid,
    pub src_id: String,
    pub ciphertext: String,
    pub created_at: DateTime<Utc>,
}

// GET /v1/messages/:node_id - wezel z dostepem do internetu odpytuje o wiadomosci
// czekajace na niego. Zwraca do 100 na raz, od najstarszej.
pub async fn poll_messages(
    State(state): State<AppState>,
    Path(node_id_hex): Path<String>,
) -> Result<Json<Vec<PendingMessage>>, ApiError> {
    let node_id = parse_node_id(&node_id_hex)?;

    let rows = sqlx::query_as::<_, (Uuid, Vec<u8>, Vec<u8>, DateTime<Utc>)>(
        "select id, src_id, ciphertext, created_at
         from messages
         where dst_id = $1
         order by created_at
         limit 100",
    )
    .bind(&node_id)
    .fetch_all(&state.db)
    .await?;

    Ok(Json(
        rows.into_iter()
            .map(|(id, src_id, ciphertext, created_at)| PendingMessage {
                id,
                src_id: hex::encode(src_id),
                ciphertext: hex::encode(ciphertext),
                created_at,
            })
            .collect(),
    ))
}

// DELETE /v1/messages/:id - wezel potwierdza ze wiadomosc dotarla, serwer moze ja skasowac.
// TODO: brak autoryzacji - dowolny klient znajacy UUID moze skasowac cudza wiadomosc z
// kolejki. Losowy UUID trudno zgadnac, ale to nie jest realna kontrola dostepu, tylko
// zaslona. Do naprawienia przed jakimkolwiek uzyciem poza wąskim gronem testowym.
pub async fn ack_message(
    State(state): State<AppState>,
    Path(id): Path<Uuid>,
) -> Result<(), ApiError> {
    let result = sqlx::query("delete from messages where id = $1")
        .bind(id)
        .execute(&state.db)
        .await?;

    if result.rows_affected() == 0 {
        return Err(ApiError::NotFound);
    }
    Ok(())
}

async fn is_blocked(state: &AppState, hwid_or_src: &[u8]) -> Result<bool, ApiError> {
    let row: Option<(Vec<u8>,)> =
        sqlx::query_as("select hwid from blocked_devices where hwid = $1")
            .bind(hwid_or_src)
            .fetch_optional(&state.db)
            .await?;
    Ok(row.is_some())
}
