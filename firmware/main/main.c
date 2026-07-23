#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "board.h"
#include "mesh.h"

static const char *TAG = "app";

// adres rozgloszeniowy do testow zasiegu - dopoki nie mamy drugiego wezla z prawdziwym
// ID pod reka, wysylamy "w eter" i patrzymy w logi czy ktos to odbiera
static const uint8_t BROADCAST_ID[PKT_NODE_ID_LEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static void on_message(const uint8_t *payload, size_t len, const uint8_t src_id[PKT_NODE_ID_LEN])
{
    // bez szyfrowania na tym etapie payload to zwykly tekst - traktujemy go tak wprost,
    // do zmiany jak dojdzie warstwa Signal w kroku 2
    ESP_LOGI(TAG, "odebrano %d B od %02x%02x%02x%02x...: \"%.*s\"",
             (int)len, src_id[0], src_id[1], src_id[2], src_id[3], (int)len, payload);
}

void app_main(void)
{
    ESP_ERROR_CHECK(board_init());
    ESP_ERROR_CHECK(mesh_init());
    mesh_set_rx_callback(on_message);

    const uint8_t *my_id = mesh_local_id();
    ESP_LOGI(TAG, "wezel wystartowal, lokalne ID: %02x%02x%02x%02x%02x%02x%02x%02x",
             my_id[0], my_id[1], my_id[2], my_id[3], my_id[4], my_id[5], my_id[6], my_id[7]);

    int64_t last_test_send_ms = 0;
    const char *test_msg = "test zasiegu z Pabianic";

    while (1) {
        mesh_poll();

        int64_t now_ms = esp_timer_get_time() / 1000;
        if (now_ms - last_test_send_ms > 30000) {
            esp_err_t err = mesh_send(BROADCAST_ID, (const uint8_t *)test_msg, strlen(test_msg));
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "wyslanie testowej wiadomosci nie wyszlo: %d", err);
            }
            last_test_send_ms = now_ms;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
