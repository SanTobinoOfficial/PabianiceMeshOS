#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "board.h"
#include "mesh.h"

static const char *TAG = "app";

// ID drugiego wezla do testow - wklej tu 8 bajtow z logu startowego drugiej plytki
// ("lokalne ID: ..."), inaczej zostaje samymi zerami i test send nic nie robi.
// Docelowo (krok 3, katalog kluczy na serwerze) node'y beda sie odkrywac same,
// na razie robimy to recznie.
static const uint8_t PEER_UNDER_TEST[PKT_NODE_ID_LEN] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void on_message(const uint8_t *payload, size_t len, const uint8_t src_id[PKT_NODE_ID_LEN])
{
    ESP_LOGI(TAG, "odebrano %d B (odszyfrowane) od %02x%02x%02x%02x...: \"%.*s\"",
             (int)len, src_id[0], src_id[1], src_id[2], src_id[3], (int)len, payload);
}

static bool peer_configured(void)
{
    uint8_t zero[PKT_NODE_ID_LEN] = { 0 };
    return memcmp(PEER_UNDER_TEST, zero, PKT_NODE_ID_LEN) != 0;
}

void app_main(void)
{
    // NVS trzyma tozsamosc Signal (identity/prekeys, patrz components/crypto) - trzeba
    // wyczyscic i zainicjowac od nowa jesli partycja jest w nieznanym/starym formacie
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    ESP_ERROR_CHECK(board_init());
    ESP_ERROR_CHECK(mesh_init());
    mesh_set_rx_callback(on_message);

    const uint8_t *my_id = mesh_local_id();
    ESP_LOGI(TAG, "wezel wystartowal, lokalne ID: %02x%02x%02x%02x%02x%02x%02x%02x",
             my_id[0], my_id[1], my_id[2], my_id[3], my_id[4], my_id[5], my_id[6], my_id[7]);

    if (!peer_configured()) {
        ESP_LOGW(TAG, "PEER_UNDER_TEST nie ustawiony - wpisz ID drugiego wezla w main.c zeby testowac wysylke");
    }

    int64_t last_test_send_ms = 0;
    const char *test_msg = "czesc z Pabianic";

    while (1) {
        mesh_poll();

        int64_t now_ms = esp_timer_get_time() / 1000;
        if (peer_configured() && now_ms - last_test_send_ms > 15000) {
            esp_err_t err = mesh_send(PEER_UNDER_TEST, (const uint8_t *)test_msg, strlen(test_msg));
            if (err == ESP_ERR_NOT_FOUND) {
                ESP_LOGI(TAG, "brak sesji z peerem jeszcze, wyslano prosbe o klucze");
            } else if (err != ESP_OK) {
                ESP_LOGW(TAG, "wyslanie testowej wiadomosci nie wyszlo: 0x%x", err);
            } else {
                ESP_LOGI(TAG, "wyslano zaszyfrowana wiadomosc testowa");
            }
            last_test_send_ms = now_ms;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
