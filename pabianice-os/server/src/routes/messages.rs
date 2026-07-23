use axum::extract::{Path, State};
use axum::Json;
use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::auth::CurrentUser;
use crate::error::ApiError;
use crate::state::AppState;

#[derive(Deserialize)]
pub struct PostMessageReq {
    pub body: String,
}

#[derive(Serialize)]
pub struct MessageResp {
    pub id: Uuid,
    pub channel_id: Uuid,
    pub author: String,
    pub body: String,
    pub created_at: DateTime<Utc>,
}

// Kazdy zalogowany moze pisac na kazdym kanale - uproszczenie w tym szkielecie,
// bez override uprawnien per-kanal (patrz pabianice-os/README.md, "czego brakuje").
pub async fn post_message(
    State(state): State<AppState>,
    user: CurrentUser,
    Path(channel_id): Path<Uuid>,
    Json(req): Json<PostMessageReq>,
) -> Result<Json<MessageResp>, ApiError> {
    let row: (Uuid, DateTime<Utc>) = sqlx::query_as(
        "insert into channel_messages (channel_id, author_id, body) values ($1, $2, $3)
         returning id, created_at",
    )
    .bind(channel_id)
    .bind(user.id)
    .bind(&req.body)
    .fetch_one(&state.db)
    .await?;

    Ok(Json(MessageResp {
        id: row.0,
        channel_id,
        author: user.username,
        body: req.body,
        created_at: row.1,
    }))
}

pub async fn list_messages(
    State(state): State<AppState>,
    _user: CurrentUser,
    Path(channel_id): Path<Uuid>,
) -> Result<Json<Vec<MessageResp>>, ApiError> {
    let rows = sqlx::query_as::<_, (Uuid, String, String, DateTime<Utc>)>(
        "select channel_messages.id, users.username, channel_messages.body,
                channel_messages.created_at
         from channel_messages
         join users on users.id = channel_messages.author_id
         where channel_messages.channel_id = $1
         order by channel_messages.created_at asc
         limit 200",
    )
    .bind(channel_id)
    .fetch_all(&state.db)
    .await?;

    Ok(Json(
        rows.into_iter()
            .map(|(id, author, body, created_at)| MessageResp {
                id,
                channel_id,
                author,
                body,
                created_at,
            })
            .collect(),
    ))
}
