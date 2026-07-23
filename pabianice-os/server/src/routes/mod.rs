pub mod admin;
pub mod auth;
pub mod channels;
pub mod messages;
pub mod users;

use axum::middleware::from_fn_with_state;
use axum::routing::{delete, get, post, put};
use axum::Router;
use tower_http::services::ServeDir;

use crate::auth::require_session;
use crate::state::AppState;

pub fn router(state: AppState, admin_panel_dir: &str) -> Router {
    let authenticated = Router::new()
        .route("/v1/auth/logout", post(auth::logout))
        .route("/v1/users", get(users::list_users).post(users::create_user))
        .route("/v1/users/:id/role", put(users::set_role))
        .route("/v1/users/:id/password", put(users::reset_password))
        .route(
            "/v1/categories",
            get(channels::list_categories).post(channels::create_category),
        )
        .route(
            "/v1/channels",
            get(channels::list_channels).post(channels::create_channel),
        )
        .route("/v1/channels/:id", delete(channels::delete_channel))
        .route(
            "/v1/channels/:id/min-post-role",
            put(channels::set_min_post_role),
        )
        .route(
            "/v1/channels/:id/messages",
            get(messages::list_messages).post(messages::post_message),
        )
        .route(
            "/v1/admin/settings",
            get(admin::get_settings).put(admin::update_settings),
        )
        .route_layer(from_fn_with_state(state.clone(), require_session));

    // panel admina to statyczne pliki (pabianice-os/admin-panel) serwowane z tego
    // samego procesu i portu co API - jeden binarny, zero CORS, prosciej dla
    // operatora ktory to sam sobie hostuje (patrz pabianice-os/README.md)
    Router::new()
        .route("/v1/auth/login", post(auth::login))
        .merge(authenticated)
        .with_state(state)
        .fallback_service(ServeDir::new(admin_panel_dir))
}
