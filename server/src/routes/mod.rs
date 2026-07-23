pub mod blocklist;
pub mod keys;
pub mod messages;

use axum::middleware::from_fn_with_state;
use axum::routing::{delete, get, put};
use axum::Router;

use crate::auth::require_admin;
use crate::state::AppState;

pub fn router(state: AppState) -> Router {
    let admin_routes = Router::new()
        .route("/v1/blocklist", get(blocklist::list_blocked))
        .route(
            "/v1/blocklist/:hwid",
            put(blocklist::block_device).delete(blocklist::unblock_device),
        )
        .route_layer(from_fn_with_state(state.clone(), require_admin));

    Router::new()
        .route(
            "/v1/keys/:node_id",
            put(keys::publish_bundle).get(keys::get_bundle),
        )
        .route(
            "/v1/messages",
            axum::routing::post(messages::submit_message),
        )
        .route("/v1/messages/:node_id", get(messages::poll_messages))
        .route(
            "/v1/messages/:node_id/id/:id",
            delete(messages::ack_message),
        )
        .merge(admin_routes)
        .with_state(state)
}
