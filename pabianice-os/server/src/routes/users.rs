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

#[cfg(test)]
mod tests {
    use super::*;
    use sqlx::PgPool;

    fn admin_user() -> CurrentUser {
        CurrentUser {
            id: Uuid::new_v4(),
            username: "admin".into(),
            role: Role::Admin,
            token: vec![],
        }
    }

    fn member_user() -> CurrentUser {
        CurrentUser {
            id: Uuid::new_v4(),
            username: "czlonek".into(),
            role: Role::Member,
            token: vec![],
        }
    }

    fn req(username: &str) -> CreateUserReq {
        CreateUserReq {
            username: username.into(),
            password: "wieczorami-po-pracy".into(),
            role: None,
        }
    }

    #[sqlx::test]
    async fn non_admin_cannot_create_user(pool: PgPool) {
        let state = AppState { db: pool };
        let result = create_user(State(state), member_user(), Json(req("nowy"))).await;
        assert!(matches!(result, Err(ApiError::Forbidden)));
    }

    #[sqlx::test]
    async fn admin_can_create_user(pool: PgPool) {
        let state = AppState { db: pool };
        let mut r = req("nowy");
        r.role = Some(Role::Moderator);
        let created = create_user(State(state), admin_user(), Json(r))
            .await
            .unwrap();
        assert_eq!(created.0.username, "nowy");
        assert_eq!(created.0.role, Role::Moderator);
    }

    #[sqlx::test]
    async fn duplicate_username_is_conflict(pool: PgPool) {
        let state = AppState { db: pool };
        let _first = create_user(State(state.clone()), admin_user(), Json(req("duplikat")))
            .await
            .unwrap();
        let second = create_user(State(state), admin_user(), Json(req("duplikat"))).await;
        assert!(matches!(second, Err(ApiError::Conflict)));
    }
}
