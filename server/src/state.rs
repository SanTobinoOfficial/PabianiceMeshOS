use redis::aio::ConnectionManager;
use sqlx::PgPool;

#[derive(Clone)]
pub struct AppState {
    pub db: PgPool,
    pub redis: ConnectionManager,
    // domyslna retencja wiadomosci - 90 dni, tabela z rozdz. 10.2 planu.
    // nadpisywalne przez RETENTION_DAYS w env, patrz main.rs
    pub retention_days: i64,
    // bearer token do endpointow /v1/blocklist/*, patrz auth.rs
    pub admin_token: String,
}
