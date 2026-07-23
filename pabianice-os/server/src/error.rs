use axum::http::StatusCode;
use axum::response::{IntoResponse, Response};
use axum::Json;
use serde_json::json;

#[derive(thiserror::Error, Debug)]
pub enum ApiError {
    #[error("nie znaleziono")]
    NotFound,
    #[error("zly request: {0}")]
    BadRequest(String),
    #[error("brak albo zla sesja")]
    Unauthorized,
    #[error("za male uprawnienia")]
    Forbidden,
    #[error("nazwa uzytkownika juz zajeta")]
    Conflict,
    #[error("blad bazy danych")]
    Db(#[from] sqlx::Error),
}

impl IntoResponse for ApiError {
    fn into_response(self) -> Response {
        let status = match &self {
            ApiError::NotFound => StatusCode::NOT_FOUND,
            ApiError::BadRequest(_) => StatusCode::BAD_REQUEST,
            ApiError::Unauthorized => StatusCode::UNAUTHORIZED,
            ApiError::Forbidden => StatusCode::FORBIDDEN,
            ApiError::Conflict => StatusCode::CONFLICT,
            ApiError::Db(e) => {
                tracing::error!("db error: {e}");
                StatusCode::INTERNAL_SERVER_ERROR
            }
        };
        (status, Json(json!({ "error": self.to_string() }))).into_response()
    }
}
