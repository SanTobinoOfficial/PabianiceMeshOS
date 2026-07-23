use axum::extract::State;
use axum::Json;
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::auth::{generate_session_token, verify_password, CurrentUser};
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

// Usuwa dokladnie ta jedna sesje (nie wszystkie sesje uzytkownika) - panel admina
// wczesniej tylko czyscil localStorage po swojej stronie, wpis w bazie zyl dalej
// az do wygasniecia (30 dni).
pub async fn logout(State(state): State<AppState>, user: CurrentUser) -> Result<(), ApiError> {
    sqlx::query("delete from sessions where token = $1")
        .bind(&user.token)
        .execute(&state.db)
        .await?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::auth::hash_password;
    use sqlx::PgPool;

    #[sqlx::test]
    async fn logout_deletes_the_session(pool: PgPool) {
        let user_id: Uuid = sqlx::query_scalar(
            "insert into users (username, password_hash, role) values ($1, $2, 'member')
             returning id",
        )
        .bind("ktos")
        .bind(hash_password("haslo-testowe-123").unwrap())
        .fetch_one(&pool)
        .await
        .unwrap();

        let state = AppState { db: pool.clone() };
        let login_resp = login(
            State(state.clone()),
            Json(LoginReq {
                username: "ktos".into(),
                password: "haslo-testowe-123".into(),
            }),
        )
        .await
        .unwrap();

        let token = hex::decode(&login_resp.0.token).unwrap();
        let current = CurrentUser {
            id: user_id,
            username: "ktos".into(),
            role: Role::Member,
            token: token.clone(),
        };

        logout(State(state), current).await.unwrap();

        let remaining: i64 = sqlx::query_scalar("select count(*) from sessions where token = $1")
            .bind(&token)
            .fetch_one(&pool)
            .await
            .unwrap();
        assert_eq!(remaining, 0);
    }
}
