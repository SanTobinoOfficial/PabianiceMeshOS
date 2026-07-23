use axum::extract::State;
use axum::Json;
use serde::{Deserialize, Serialize};

use crate::auth::CurrentUser;
use crate::error::ApiError;
use crate::roles::Role;
use crate::state::AppState;

#[derive(Serialize, Deserialize)]
pub struct ServerSettings {
    pub message_retention_days: i32,
    pub federation_enabled: bool,
}

pub async fn get_settings(
    State(state): State<AppState>,
    user: CurrentUser,
) -> Result<Json<ServerSettings>, ApiError> {
    user.require(Role::Moderator)?;

    let row: (i32, bool) =
        sqlx::query_as("select message_retention_days, federation_enabled from server_settings")
            .fetch_one(&state.db)
            .await?;

    Ok(Json(ServerSettings {
        message_retention_days: row.0,
        federation_enabled: row.1,
    }))
}

// Federacja to na razie tylko flaga w bazie - sam protokol federacji miedzy serwerami
// (rozdz. 10.4.2 planu) nie jest zaimplementowany. Wlaczenie tego ustawienia niczego
// jeszcze funkcjonalnie nie zmienia poza odnotowaniem decyzji operatora na przyszlosc.
pub async fn update_settings(
    State(state): State<AppState>,
    user: CurrentUser,
    Json(req): Json<ServerSettings>,
) -> Result<(), ApiError> {
    user.require(Role::Admin)?;

    sqlx::query("update server_settings set message_retention_days = $1, federation_enabled = $2")
        .bind(req.message_retention_days)
        .bind(req.federation_enabled)
        .execute(&state.db)
        .await?;

    Ok(())
}
