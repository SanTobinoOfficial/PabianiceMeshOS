use std::time::Duration;

use sqlx::PgPool;

// Sprzatanie wiadomosci po uplywie okresu retencji (domyslnie 90 dni, patrz rozdz. 10.2
// planu - "automatyczne czyszczenie retencji" jako osobne zadanie cykliczne). Tu w
// procesie zamiast osobnego cron joba, prosciej na ten etap.
pub fn spawn_cleanup_task(db: PgPool) {
    tokio::spawn(async move {
        let mut interval = tokio::time::interval(Duration::from_secs(60 * 60));
        loop {
            interval.tick().await;
            match sqlx::query("delete from messages where expires_at < now()")
                .execute(&db)
                .await
            {
                Ok(res) => {
                    if res.rows_affected() > 0 {
                        tracing::info!(
                            "retencja: usunieto {} przeterminowanych wiadomosci",
                            res.rows_affected()
                        );
                    }
                }
                Err(e) => tracing::error!("retencja: czyszczenie nie wyszlo: {e}"),
            }
        }
    });
}
