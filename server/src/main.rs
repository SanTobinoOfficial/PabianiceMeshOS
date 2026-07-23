mod auth;
mod error;
mod nodeid;
mod retention;
mod routes;
mod state;

use std::env;

use redis::aio::ConnectionManager;
use sqlx::postgres::PgPoolOptions;

use state::AppState;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    dotenvy::dotenv().ok(); // nie ma pliku .env - trudno, i tak leci z env systemowego

    tracing_subscriber::fmt()
        .with_env_filter(tracing_subscriber::EnvFilter::from_default_env())
        .init();

    let database_url = env::var("DATABASE_URL")
        .unwrap_or_else(|_| "postgres://postgres:postgres@localhost:5432/pabianice".into());
    let redis_url = env::var("REDIS_URL").unwrap_or_else(|_| "redis://127.0.0.1:6379".into());
    let retention_days: i64 = env::var("RETENTION_DAYS")
        .ok()
        .and_then(|v| v.parse().ok())
        .unwrap_or(90);
    let bind_addr = env::var("BIND_ADDR").unwrap_or_else(|_| "0.0.0.0:8080".into());
    let admin_token = env::var("ADMIN_TOKEN")
        .map_err(|_| anyhow::anyhow!("ADMIN_TOKEN nie ustawiony - wymagany do /v1/blocklist"))?;

    let db = PgPoolOptions::new()
        .max_connections(10)
        .connect(&database_url)
        .await?;
    sqlx::migrate!("./migrations").run(&db).await?;

    let redis_client = redis::Client::open(redis_url)?;
    let redis = ConnectionManager::new(redis_client).await?;

    retention::spawn_cleanup_task(db.clone());

    let state = AppState {
        db,
        redis,
        retention_days,
        admin_token,
    };

    let app = routes::router(state);

    tracing::info!("nasluchuje na {bind_addr}");
    let listener = tokio::net::TcpListener::bind(&bind_addr).await?;
    axum::serve(listener, app).await?;

    Ok(())
}
