use axum::extract::{Request, State};
use axum::middleware::Next;
use axum::response::Response;
use subtle::ConstantTimeEq;

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

    // staloczasowe porownanie - zwykle == na sekrecie potrafi zdradzic dlugosc
    // wspolnego prefiksu przez czas odpowiedzi (timing attack), tanie zabezpieczenie
    // nawet jesli w praktyce trudne do wykorzystania przez siec. ct_eq sam obsluguje
    // rozne dlugosci (Choice::from(0)), bez wczesniejszego porownania dlugosci.
    match token {
        Some(t) if bool::from(t.as_bytes().ct_eq(state.admin_token.as_bytes())) => {
            Ok(next.run(req).await)
        }
        _ => Err(ApiError::Unauthorized),
    }
}
