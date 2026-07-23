use axum::extract::{Path, State};
use axum::Json;
use chrono::{DateTime, Duration, Utc};
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::error::ApiError;
use crate::nodeid::parse_node_id;
use crate::state::AppState;

// Per-nadawca okno 60s, limit taki sam jak w mesh.c firmware (RATE_LIMIT_MAX_PER_WIN) -
// bez tego jeden zepsuty/zlosliwy klient moze zalac kolejke serwera tak samo jak
// zalewalby eter w mesh (rozdz. 7.2 planu). Redis traktowany jako best-effort, tak
// samo jak nudge przy submit_message - jesli akurat nie dziala, przepuszczamy zamiast
// blokowac caly ruch (Redis jest tu infrastruktura pomocnicza, nie krytyczna).
const RATE_LIMIT_WINDOW_SECS: i64 = 60;
const RATE_LIMIT_MAX_PER_WINDOW: i64 = 20;

async fn check_rate_limit(state: &AppState, src_id: &[u8]) -> bool {
    let mut conn = state.redis.clone();
    let key = format!("ratelimit:submit:{}", hex::encode(src_id));

    let count: redis::RedisResult<i64> = redis::cmd("INCR").arg(&key).query_async(&mut conn).await;
    match count {
        Ok(1) => {
            let _: redis::RedisResult<()> = redis::cmd("EXPIRE")
                .arg(&key)
                .arg(RATE_LIMIT_WINDOW_SECS)
                .query_async(&mut conn)
                .await;
            true
        }
        Ok(n) => n <= RATE_LIMIT_MAX_PER_WINDOW,
        Err(e) => {
            tracing::warn!("rate limit: redis niedostepny, przepuszczam ({e})");
            true
        }
    }
}

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

    if !check_rate_limit(&state, &src_id).await {
        return Err(ApiError::RateLimited);
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

// DELETE /v1/messages/:node_id/id/:id - wezel potwierdza ze wiadomosc dotarla, serwer
// moze ja skasowac. Wymaga podania wlasnego node_id (dst_id), nie tylko UUID wiadomosci -
// wczesniej sama znajomosc losowego UUID wystarczala do skasowania cudzej wiadomosci z
// kolejki (zaslona, nie realna kontrola dostepu). Teraz DELETE dziala tylko jesli node_id
// w URL faktycznie zgadza sie z dst_id tej wiadomosci w bazie.
pub async fn ack_message(
    State(state): State<AppState>,
    Path((node_id_hex, id)): Path<(String, Uuid)>,
) -> Result<(), ApiError> {
    let node_id = parse_node_id(&node_id_hex)?;

    let result = sqlx::query("delete from messages where id = $1 and dst_id = $2")
        .bind(id)
        .bind(&node_id)
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::state::test_state;
    use sqlx::PgPool;

    fn submit_req(dst: &str, src: &str) -> SubmitMessageReq {
        SubmitMessageReq {
            dst_id: dst.into(),
            src_id: src.into(),
            ciphertext: "beef".into(),
        }
    }

    #[sqlx::test]
    async fn submit_then_poll_roundtrip(pool: PgPool) {
        let state = test_state(pool).await;
        let _ = submit_message(
            State(state.clone()),
            Json(submit_req("1111111111111111", "2222222222222222")),
        )
        .await
        .unwrap();

        let pending = poll_messages(State(state), Path("1111111111111111".into()))
            .await
            .unwrap();
        assert_eq!(pending.0.len(), 1);
        assert_eq!(pending.0[0].src_id, "2222222222222222");
    }

    #[sqlx::test]
    async fn ack_with_wrong_node_id_does_not_delete(pool: PgPool) {
        let state = test_state(pool).await;
        let submitted = submit_message(
            State(state.clone()),
            Json(submit_req("3333333333333333", "4444444444444444")),
        )
        .await
        .unwrap();

        let wrong_ack = ack_message(
            State(state.clone()),
            Path(("5555555555555555".into(), submitted.0.id)),
        )
        .await;
        assert!(matches!(wrong_ack, Err(ApiError::NotFound)));

        let still_pending = poll_messages(State(state), Path("3333333333333333".into()))
            .await
            .unwrap();
        assert_eq!(still_pending.0.len(), 1);
    }

    #[sqlx::test]
    async fn ack_with_correct_node_id_deletes(pool: PgPool) {
        let state = test_state(pool).await;
        let submitted = submit_message(
            State(state.clone()),
            Json(submit_req("3333333333333333", "4444444444444444")),
        )
        .await
        .unwrap();

        ack_message(
            State(state.clone()),
            Path(("3333333333333333".into(), submitted.0.id)),
        )
        .await
        .unwrap();

        let remaining = poll_messages(State(state), Path("3333333333333333".into()))
            .await
            .unwrap();
        assert_eq!(remaining.0.len(), 0);
    }

    #[sqlx::test]
    async fn blocked_sender_cannot_submit(pool: PgPool) {
        let src_id = parse_node_id("6666666666666666").unwrap();
        sqlx::query("insert into blocked_devices (hwid) values ($1)")
            .bind(&src_id)
            .execute(&pool)
            .await
            .unwrap();

        let state = test_state(pool).await;
        let result = submit_message(
            State(state),
            Json(submit_req("7777777777777777", "6666666666666666")),
        )
        .await;
        assert!(matches!(result, Err(ApiError::Blocked)));
    }

    #[sqlx::test]
    async fn rate_limit_blocks_after_threshold(pool: PgPool) {
        let state = test_state(pool).await;
        for _ in 0..RATE_LIMIT_MAX_PER_WINDOW {
            let _ = submit_message(
                State(state.clone()),
                Json(submit_req("9999999999999999", "8888888888888888")),
            )
            .await
            .unwrap();
        }

        let over_limit = submit_message(
            State(state),
            Json(submit_req("9999999999999999", "8888888888888888")),
        )
        .await;
        assert!(matches!(over_limit, Err(ApiError::RateLimited)));
    }
}
