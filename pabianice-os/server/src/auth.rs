use argon2::password_hash::{PasswordHasher, PasswordVerifier, SaltString};
use argon2::{Argon2, PasswordHash};
use axum::extract::{FromRequestParts, Request, State};
use axum::http::request::Parts;
use axum::middleware::Next;
use axum::response::Response;
use rand::rngs::OsRng;
use rand::RngCore;
use uuid::Uuid;

use crate::error::ApiError;
use crate::roles::Role;
use crate::state::AppState;

pub fn hash_password(plain: &str) -> Result<String, ApiError> {
    let salt = SaltString::generate(&mut OsRng);
    Argon2::default()
        .hash_password(plain.as_bytes(), &salt)
        .map(|h| h.to_string())
        .map_err(|_| ApiError::BadRequest("nie udalo sie zahashowac hasla".into()))
}

pub fn verify_password(plain: &str, hash: &str) -> bool {
    let Ok(parsed) = PasswordHash::new(hash) else {
        return false;
    };
    Argon2::default()
        .verify_password(plain.as_bytes(), &parsed)
        .is_ok()
}

// 32 losowe bajty, nosimy jako hex w naglowku Authorization - ten sam wzorzec co
// bearer token administratora w /server (patrz server/src/auth.rs), tylko per-uzytkownik
// i trzymany w tabeli sessions zamiast jednego statycznego tokena w env.
pub fn generate_session_token() -> Vec<u8> {
    let mut buf = [0u8; 32];
    OsRng.fill_bytes(&mut buf);
    buf.to_vec()
}

#[derive(Debug, Clone)]
pub struct CurrentUser {
    pub id: Uuid,
    pub username: String,
    pub role: Role,
}

impl CurrentUser {
    pub fn require(&self, min: Role) -> Result<(), ApiError> {
        if self.role.has_at_least(min) {
            Ok(())
        } else {
            Err(ApiError::Forbidden)
        }
    }
}

#[axum::async_trait]
impl<S> FromRequestParts<S> for CurrentUser
where
    S: Send + Sync,
{
    type Rejection = ApiError;

    async fn from_request_parts(parts: &mut Parts, _state: &S) -> Result<Self, Self::Rejection> {
        parts
            .extensions
            .get::<CurrentUser>()
            .cloned()
            .ok_or(ApiError::Unauthorized)
    }
}

pub async fn require_session(
    State(state): State<AppState>,
    mut req: Request,
    next: Next,
) -> Result<Response, ApiError> {
    let token_hex = req
        .headers()
        .get(axum::http::header::AUTHORIZATION)
        .and_then(|v| v.to_str().ok())
        .and_then(|v| v.strip_prefix("Bearer "))
        .ok_or(ApiError::Unauthorized)?;

    let token = hex::decode(token_hex).map_err(|_| ApiError::Unauthorized)?;

    let row = sqlx::query_as::<_, (Uuid, String, Role)>(
        "select users.id, users.username, users.role
         from sessions join users on users.id = sessions.user_id
         where sessions.token = $1 and sessions.expires_at > now()",
    )
    .bind(&token)
    .fetch_optional(&state.db)
    .await?;

    let (id, username, role) = row.ok_or(ApiError::Unauthorized)?;
    req.extensions_mut()
        .insert(CurrentUser { id, username, role });

    Ok(next.run(req).await)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn hashes_and_verifies_correct_password() {
        let hash = hash_password("wieczorami-po-pracy").unwrap();
        assert!(verify_password("wieczorami-po-pracy", &hash));
    }

    #[test]
    fn rejects_wrong_password() {
        let hash = hash_password("wieczorami-po-pracy").unwrap();
        assert!(!verify_password("cos-innego", &hash));
    }

    #[test]
    fn session_tokens_are_32_bytes_and_not_repeated() {
        let a = generate_session_token();
        let b = generate_session_token();
        assert_eq!(a.len(), 32);
        assert_ne!(a, b);
    }
}
