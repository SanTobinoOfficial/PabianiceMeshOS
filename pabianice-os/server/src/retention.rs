use std::time::Duration;

use sqlx::PgPool;

// Czyszczenie starych wiadomosci kanalowych wg message_retention_days z server_settings
// (rozdz. 10.4.1 planu - retencja niezalezna od /server, decyzja operatora). 0 = bez
// limitu. Ustawienie moze sie zmienic w trakcie dzialania (panel admina), wiec
// odczytujemy je na kazdym ticku zamiast raz przy starcie.
pub fn spawn_cleanup_task(db: PgPool) {
    tokio::spawn(async move {
        let mut interval = tokio::time::interval(Duration::from_secs(60 * 60));
        loop {
            interval.tick().await;

            let retention_days: i32 =
                match sqlx::query_scalar("select message_retention_days from server_settings")
                    .fetch_one(&db)
                    .await
                {
                    Ok(v) => v,
                    Err(e) => {
                        tracing::error!("retencja: nie udalo sie odczytac ustawien: {e}");
                        continue;
                    }
                };

            if retention_days <= 0 {
                continue;
            }

            match sqlx::query(
                "delete from channel_messages where created_at < now() - make_interval(days => $1)",
            )
            .bind(retention_days)
            .execute(&db)
            .await
            {
                Ok(res) => {
                    if res.rows_affected() > 0 {
                        tracing::info!(
                            "retencja: usunieto {} przeterminowanych wiadomosci kanalowych",
                            res.rows_affected()
                        );
                    }
                }
                Err(e) => tracing::error!("retencja: czyszczenie nie wyszlo: {e}"),
            }
        }
    });
}
