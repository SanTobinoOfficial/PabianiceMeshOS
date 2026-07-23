use axum::extract::State;
use axum::Json;
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::auth::{generate_session_token, verify_password};
use crate::error::ApiError;
use crate::roles::Role;
use crate::state::AppState;

#[derive(Deserialize)]
pub struct LoginReq {
    pub username: String,
    pub password: String,
}

#[derive(Serialize)]
pub struct LoginResp {
    pub token: String,
    pub role: Role,
}

pub async fn login(
    State(state): State<AppState>,
    Json(req): Json<LoginReq>,
) -> Result<Json<LoginResp>, ApiError> {
    let row = sqlx::query_as::<_, (Uuid, String, Role)>(
        "select id, password_hash, role from users where username = $1",
    )
    .bind(&req.username)
    .fetch_optional(&state.db)
    .await?;

    let (user_id, password_hash, role) = row.ok_or(ApiError::Unauthorized)?;
    if !verify_password(&req.password, &password_hash) {
        return Err(ApiError::Unauthorized);
    }

    let token = generate_session_token();
    let expires_at = chrono::Utc::now() + chrono::Duration::days(30);
    sqlx::query("insert into sessions (token, user_id, expires_at) values ($1, $2, $3)")
        .bind(&token)
        .bind(user_id)
        .bind(expires_at)
        .execute(&state.db)
        .await?;

    Ok(Json(LoginResp {
        token: hex::encode(token),
        role,
    }))
}
