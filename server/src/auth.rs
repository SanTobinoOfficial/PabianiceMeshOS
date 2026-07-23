use axum::extract::{Request, State};
use axum::middleware::Next;
use axum::response::Response;

use crate::error::ApiError;
use crate::state::AppState;

// Blokada admin-only endpointow (rozdz. 7.3.1 planu - blokowanie HWID w trybie
// scentralizowanym). Prosty bearer token zamiast pelnego panelu z 2FA - ten
// drugi to Pabianice OS (rozdz. 10.4 planu), swiadomie pozniejszy etap. Na razie
// to jedyne co stoi miedzy publicznym API a mozliwoscia blokowania urzadzen.
pub async fn require_admin(
    State(state): State<AppState>,
    req: Request,
    next: Next,
) -> Result<Response, ApiError> {
    let token = req
        .headers()
        .get(axum::http::header::AUTHORIZATION)
        .and_then(|v| v.to_str().ok())
        .and_then(|v| v.strip_prefix("Bearer "));

    match token {
        Some(t) if t == state.admin_token => Ok(next.run(req).await),
        _ => Err(ApiError::Unauthorized),
    }
}
