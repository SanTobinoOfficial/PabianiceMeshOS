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

// Wspolny helper do testow integracyjnych (#[sqlx::test]) - kazdy test dostaje
// swoja tymczasowa baze od sqlx, ale dzieli jeden lokalny Redis (bezpieczne, bo
// testy albo go nie dotykaja, albo uzywaja losowych kluczy per test).
#[cfg(test)]
pub async fn test_state(db: PgPool) -> AppState {
    let client = redis::Client::open("redis://127.0.0.1:6379").expect("redis url");
    let redis = ConnectionManager::new(client)
        .await
        .expect("polacz z lokalnym redis (wymagany do testow)");
    AppState {
        db,
        redis,
        retention_days: 90,
        admin_token: "test-admin-token".into(),
    }
}
