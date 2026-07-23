mod auth;
mod error;
mod roles;
mod routes;
mod state;

use std::env;

use sqlx::postgres::PgPoolOptions;

use auth::hash_password;
use roles::Role;
use state::AppState;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    dotenvy::dotenv().ok();

    tracing_subscriber::fmt()
        .with_env_filter(tracing_subscriber::EnvFilter::from_default_env())
        .init();

    let database_url = env::var("DATABASE_URL")
        .unwrap_or_else(|_| "postgres://postgres:postgres@localhost:5432/pabianice_os".into());
    let bind_addr = env::var("BIND_ADDR").unwrap_or_else(|_| "0.0.0.0:8081".into());
    let admin_panel_dir = env::var("ADMIN_PANEL_DIR").unwrap_or_else(|_| "../admin-panel".into());

    let db = PgPoolOptions::new()
        .max_connections(10)
        .connect(&database_url)
        .await?;
    sqlx::migrate!("./migrations").run(&db).await?;

    bootstrap_admin(&db).await?;

    let state = AppState { db };
    let app = routes::router(state, &admin_panel_dir);

    tracing::info!("Pabianice OS nasluchuje na {bind_addr}");
    let listener = tokio::net::TcpListener::bind(&bind_addr).await?;
    axum::serve(listener, app).await?;

    Ok(())
}

// Pierwsze konto administratora - potrzebne bo nie ma publicznej samorejestracji
// (routes/users.rs). Odpala sie tylko gdy tabela users jest pusta, zeby restart
// serwera nie probowal tworzyc konta na nowo przy kazdym starcie.
async fn bootstrap_admin(db: &sqlx::PgPool) -> anyhow::Result<()> {
    let count: i64 = sqlx::query_scalar("select count(*) from users")
        .fetch_one(db)
        .await?;
    if count > 0 {
        return Ok(());
    }

    let username = env::var("ADMIN_BOOTSTRAP_USERNAME")
        .map_err(|_| anyhow::anyhow!("tabela users jest pusta i brak ADMIN_BOOTSTRAP_USERNAME - ustaw go zeby stworzyc pierwsze konto administratora"))?;
    let password = env::var("ADMIN_BOOTSTRAP_PASSWORD")
        .map_err(|_| anyhow::anyhow!("tabela users jest pusta i brak ADMIN_BOOTSTRAP_PASSWORD - ustaw go zeby stworzyc pierwsze konto administratora"))?;

    let password_hash = hash_password(&password).map_err(|e| anyhow::anyhow!("{e}"))?;
    sqlx::query("insert into users (username, password_hash, role) values ($1, $2, $3)")
        .bind(&username)
        .bind(password_hash)
        .bind(Role::Admin)
        .execute(db)
        .await?;

    tracing::info!("utworzono pierwsze konto administratora: {username}");
    Ok(())
}
