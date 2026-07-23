use axum::extract::{Path, State};
use axum::Json;
use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::auth::CurrentUser;
use crate::error::ApiError;
use crate::roles::Role;
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

pub async fn post_message(
    State(state): State<AppState>,
    user: CurrentUser,
    Path(channel_id): Path<Uuid>,
    Json(req): Json<PostMessageReq>,
) -> Result<Json<MessageResp>, ApiError> {
    let min_post_role: Role =
        sqlx::query_scalar("select min_post_role from channels where id = $1")
            .bind(channel_id)
            .fetch_optional(&state.db)
            .await?
            .ok_or(ApiError::NotFound)?;
    user.require(min_post_role)?;

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

#[cfg(test)]
mod tests {
    use super::*;
    use sqlx::PgPool;

    async fn make_user(pool: &PgPool, role: Role) -> CurrentUser {
        let id: Uuid = sqlx::query_scalar(
            "insert into users (username, password_hash, role) values ($1, 'x', $2) returning id",
        )
        .bind(format!("user-{}", Uuid::new_v4()))
        .bind(role)
        .fetch_one(pool)
        .await
        .unwrap();
        CurrentUser {
            id,
            username: "test".into(),
            role,
            token_hash: vec![],
        }
    }

    async fn make_channel(pool: &PgPool, min_post_role: Role) -> Uuid {
        sqlx::query_scalar(
            "insert into channels (name, min_post_role) values ('ogloszenia', $1) returning id",
        )
        .bind(min_post_role)
        .fetch_one(pool)
        .await
        .unwrap()
    }

    #[sqlx::test]
    async fn member_cannot_post_to_admin_only_channel(pool: PgPool) {
        let member = make_user(&pool, Role::Member).await;
        let channel_id = make_channel(&pool, Role::Admin).await;
        let state = AppState { db: pool };

        let result = post_message(
            State(state),
            member,
            Path(channel_id),
            Json(PostMessageReq {
                body: "czesc".into(),
            }),
        )
        .await;

        assert!(matches!(result, Err(ApiError::Forbidden)));
    }

    #[sqlx::test]
    async fn admin_can_post_to_admin_only_channel(pool: PgPool) {
        let admin = make_user(&pool, Role::Admin).await;
        let channel_id = make_channel(&pool, Role::Admin).await;
        let state = AppState { db: pool };

        let result = post_message(
            State(state),
            admin,
            Path(channel_id),
            Json(PostMessageReq {
                body: "ogloszenie".into(),
            }),
        )
        .await
        .unwrap();

        assert_eq!(result.0.body, "ogloszenie");
    }
}
