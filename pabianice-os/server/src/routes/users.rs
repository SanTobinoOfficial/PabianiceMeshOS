use axum::extract::{Path, State};
use axum::Json;
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::auth::{hash_password, CurrentUser};
use crate::error::ApiError;
use crate::roles::Role;
use crate::state::AppState;

#[derive(Deserialize)]
pub struct CreateUserReq {
    pub username: String,
    pub password: String,
    pub role: Option<Role>,
}

#[derive(Serialize)]
pub struct UserResp {
    pub id: Uuid,
    pub username: String,
    pub role: Role,
}

// Tworzenie kont robi admin - brak publicznej samorejestracji, to prywatny serwer
// spolecznosciowy operatora, nie otwarta platforma (rozdz. 10.4 planu).
pub async fn create_user(
    State(state): State<AppState>,
    user: CurrentUser,
    Json(req): Json<CreateUserReq>,
) -> Result<Json<UserResp>, ApiError> {
    user.require(Role::Admin)?;

    let password_hash = hash_password(&req.password)?;
    let role = req.role.unwrap_or(Role::Member);

    let id: Uuid = sqlx::query_scalar(
        "insert into users (username, password_hash, role) values ($1, $2, $3) returning id",
    )
    .bind(&req.username)
    .bind(password_hash)
    .bind(role)
    .fetch_one(&state.db)
    .await
    .map_err(|e| match &e {
        sqlx::Error::Database(db) if db.is_unique_violation() => ApiError::Conflict,
        _ => ApiError::Db(e),
    })?;

    Ok(Json(UserResp {
        id,
        username: req.username,
        role,
    }))
}

pub async fn list_users(
    State(state): State<AppState>,
    user: CurrentUser,
) -> Result<Json<Vec<UserResp>>, ApiError> {
    user.require(Role::Moderator)?;

    let rows = sqlx::query_as::<_, (Uuid, String, Role)>(
        "select id, username, role from users order by created_at asc",
    )
    .fetch_all(&state.db)
    .await?;

    Ok(Json(
        rows.into_iter()
            .map(|(id, username, role)| UserResp { id, username, role })
            .collect(),
    ))
}

#[derive(Deserialize)]
pub struct SetRoleReq {
    pub role: Role,
}

pub async fn set_role(
    State(state): State<AppState>,
    user: CurrentUser,
    Path(target_id): Path<Uuid>,
    Json(req): Json<SetRoleReq>,
) -> Result<(), ApiError> {
    user.require(Role::Admin)?;

    let result = sqlx::query("update users set role = $1 where id = $2")
        .bind(req.role)
        .bind(target_id)
        .execute(&state.db)
        .await?;

    if result.rows_affected() == 0 {
        return Err(ApiError::NotFound);
    }
    Ok(())
}
