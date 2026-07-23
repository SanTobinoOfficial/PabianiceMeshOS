#include "mesh.h"
#include "pkt.h"
#include "dedup.h"
#include "presence.h"
#include "sx1262.h"

#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "esp_timer.h"

static const char *TAG = "mesh";

static uint8_t s_local_id[PKT_NODE_ID_LEN];
static mesh_rx_cb_t s_rx_cb = NULL;

// Rate-limit per nadawca - bez tego jeden zepsuty albo zlosliwy wezel potrafi zalac
// cala siec floodingiem i zjesc caly dostepny czas antenowy wszystkim wokolo (rozdz. 7.2).
#define RATE_LIMIT_WINDOW_US   (60LL * 1000000)
#define RATE_LIMIT_MAX_PER_WIN 20
#define RATE_TABLE_SIZE        32

typedef struct {
    uint8_t node_id[PKT_NODE_ID_LEN];
    int64_t window_start_us;
    int count;
    bool used;
} rate_entry_t;

static rate_entry_t s_rate_table[RATE_TABLE_SIZE];

static bool rate_limited(const uint8_t node_id[PKT_NODE_ID_LEN])
{
    int64_t now = esp_timer_get_time();
    int free_slot = -1;

    for (int i = 0; i < RATE_TABLE_SIZE; i++) {
        if (s_rate_table[i].used && memcmp(s_rate_table[i].node_id, node_id, PKT_NODE_ID_LEN) == 0) {
            if (now - s_rate_table[i].window_start_us > RATE_LIMIT_WINDOW_US) {
                s_rate_table[i].window_start_us = now;
                s_rate_table[i].count = 0;
            }
            s_rate_table[i].count++;
            return s_rate_table[i].count > RATE_LIMIT_MAX_PER_WIN;
        }
        if (!s_rate_table[i].used && free_slot < 0) {
            free_slot = i;
        }
    }

    if (free_slot < 0) {
        free_slot = 0; // tabela pelna - TODO eviction madrzejsza niz "nadpisz pierwszy z brzegu"
    }
    memcpy(s_rate_table[free_slot].node_id, node_id, PKT_NODE_ID_LEN);
    s_rate_table[free_slot].window_start_us = now;
    s_rate_table[free_slot].count = 1;
    s_rate_table[free_slot].used = true;
    return false;
}

static void derive_local_id(void)
{
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);

    // TODO(krok 2): to jest prowizorka dopoki nie ma kluczy Signal - docelowo src_id/dst_id
    // maja byc skrotem klucza publicznego (rozdz. 5.3 i 6.2 planu), MAC odpada calkowicie
    memcpy(s_local_id, mac, 6);
    s_local_id[6] = 0xAA;
    s_local_id[7] = 0xBB;
}

static void send_beacon(void)
{
    pkt_hdr_t hdr = {
        .type = PKT_TYPE_BEACON,
        .ttl = 1, // beacon = tylko dla bezposrednich sasiadow, floodowanie obecnosci nie ma sensu
        .timestamp = (uint32_t)(esp_timer_get_time() / 1000000),
    };
    esp_fill_random(hdr.msg_id, PKT_MSG_ID_LEN);
    memcpy(hdr.src_id, s_local_id, PKT_NODE_ID_LEN);
    memset(hdr.dst_id, 0, PKT_NODE_ID_LEN);

    uint8_t wire[PKT_HDR_LEN];
    size_t wire_len = pkt_encode(wire, sizeof(wire), &hdr, NULL, 0);
    if (wire_len) {
        sx1262_send(wire, wire_len, 1000);
    }
    presence_beacon_reset_timer();
}

static void on_radio_rx(const uint8_t *buf, size_t len, int16_t rssi, int8_t snr)
{
    pkt_hdr_t hdr;
    const uint8_t *payload;
    if (!pkt_decode(buf, len, &hdr, &payload)) {
        ESP_LOGW(TAG, "krzywy pakiet (%d B), odrzucam", (int)len);
        return;
    }

    if (memcmp(hdr.src_id, s_local_id, PKT_NODE_ID_LEN) == 0) {
        return; // wlasne echo - nie powinno sie zdarzac, ale sanity check jest tani
    }

    if (rate_limited(hdr.src_id)) {
        ESP_LOGW(TAG, "rate limit dla nadawcy, dropuje pakiet (rssi=%d)", rssi);
        return;
    }

    presence_touch(hdr.src_id);

    if (hdr.type == PKT_TYPE_BEACON) {
        return; // presence_touch juz zrobil swoje
    }

    if (dedup_check_and_mark(hdr.msg_id)) {
        return; // ktos inny juz to rozgloscil, my tez to juz widzielismy
    }

    bool for_us = memcmp(hdr.dst_id, s_local_id, PKT_NODE_ID_LEN) == 0;
    if (for_us && s_rx_cb) {
        s_rx_cb(payload, hdr.payload_len, hdr.src_id);
    }

    // Flooding: pakiet leci dalej niezaleznie czy byl dla nas - tak dziala epidemic
    // routing, ktos inny w zasiegu tez moze go potrzebowac.
    // TODO (rozdz. 5.2): jak tabela obecnosci pokazuje ze odbiorca jest juz znany,
    // dalo by sie tu skrocic flooding zamiast rozglaszac do calej sieci - na razie
    // zostaje najprostszy wariant, optymalizacja routingu na pozniej
    if (hdr.ttl > 1) {
        hdr.ttl--;
        uint8_t wire[PKT_MAX_WIRE_LEN];
        size_t wire_len = pkt_encode(wire, sizeof(wire), &hdr, payload, hdr.payload_len);
        if (wire_len) {
            sx1262_send(wire, wire_len, 1000);
        }
    }
}

esp_err_t mesh_init(void)
{
    derive_local_id();
    dedup_init();
    presence_init();
    memset(s_rate_table, 0, sizeof(s_rate_table));

    sx1262_config_t radio_cfg = {
        .freq_hz = 868100000,
        .sf = 7,
        .bw = SX1262_BW_125,
        .cr = SX1262_CR_4_5,
        // 14 dBm to bezpieczny start w ISM 868 - podnies dopiero po sprawdzeniu limitu
        // ERP dla wybranego podpasma w tabeli UKE (rozdz. 3.1 planu)
        .tx_power_dbm = 14,
    };
    esp_err_t err = sx1262_init(&radio_cfg);
    if (err != ESP_OK) {
        return err;
    }

    return sx1262_start_rx(on_radio_rx);
}

esp_err_t mesh_send(const uint8_t dst_id[PKT_NODE_ID_LEN], const uint8_t *payload, uint8_t len)
{
    if (len > PKT_MAX_PAYLOAD) {
        return ESP_ERR_INVALID_SIZE;
    }

    pkt_hdr_t hdr = {
        .type = PKT_TYPE_DATA,
        .ttl = PKT_TTL_DEFAULT,
        .timestamp = (uint32_t)(esp_timer_get_time() / 1000000),
    };
    esp_fill_random(hdr.msg_id, PKT_MSG_ID_LEN);
    memcpy(hdr.src_id, s_local_id, PKT_NODE_ID_LEN);
    memcpy(hdr.dst_id, dst_id, PKT_NODE_ID_LEN);

    // zeby wlasny pakiet, gdyby wrocil odbity od sasiada, nie zostal wziety za nowy
    dedup_check_and_mark(hdr.msg_id);

    uint8_t wire[PKT_MAX_WIRE_LEN];
    size_t wire_len = pkt_encode(wire, sizeof(wire), &hdr, payload, len);
    if (!wire_len) {
        return ESP_ERR_INVALID_SIZE;
    }

    return sx1262_send(wire, wire_len, 2000);
}

void mesh_set_rx_callback(mesh_rx_cb_t cb)
{
    s_rx_cb = cb;
}

void mesh_poll(void)
{
    if (presence_beacon_due()) {
        send_beacon();
    }
}

const uint8_t *mesh_local_id(void)
{
    return s_local_id;
}
