#include "mesh.h"
#include "pkt.h"
#include "dedup.h"
#include "presence.h"
#include "sx1262.h"
#include "pcrypto.h"

#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "esp_timer.h"

static const char *TAG = "mesh";

// subtypy payloadu dla PKT_TYPE_KEY_BUNDLE - pierwszy bajt payloadu, reszta to dane
#define KEY_BUNDLE_REQUEST  0x00
#define KEY_BUNDLE_RESPONSE 0x01

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
        // tabela pelna - zwalniamy wpis z najstarszym window_start_us (ten sam wzorzec
        // co presence_touch w presence.c), nie zawsze slot 0. Bez tego nowy nadawca
        // zawsze kasowalby licznik konkretnie sledzonego node'a (np. wlasnie
        // ukaranego za przekroczenie limitu) zamiast prawdziwie najstarszego wpisu
        int oldest = 0;
        for (int i = 1; i < RATE_TABLE_SIZE; i++) {
            if (s_rate_table[i].window_start_us < s_rate_table[oldest].window_start_us) {
                oldest = i;
            }
        }
        free_slot = oldest;
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

    // TODO: adres routingu na poziomie mesh nadal jest z MAC, nie ze skrotu klucza
    // publicznego jak w rozdz. 5.3 planu - dziala, bo Signal Protocol i tak trzyma
    // sesje pod adresem (nie pod samym kluczem), ale traci sie mozliwosc weryfikacji
    // "kto to jest" bez wczesniejszego TOFU. Do ogarniecia jak dojdzie katalog kluczy (krok 3).
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

static void send_key_bundle_frame(const uint8_t dst_id[PKT_NODE_ID_LEN], uint8_t subtype,
                                   const uint8_t *extra, size_t extra_len)
{
    pkt_hdr_t hdr = {
        .type = PKT_TYPE_KEY_BUNDLE,
        // tylko bezposredni sasiad, bez routingu przez posrednikow - katalogu kluczy
        // na serwerze (rozdz. 10.1 planu) jeszcze nie ma, wiec na razie wymieniamy
        // sie bundlem wprost miedzy dwoma wezlami ktore sie slysza
        .ttl = 1,
        .timestamp = (uint32_t)(esp_timer_get_time() / 1000000),
    };
    esp_fill_random(hdr.msg_id, PKT_MSG_ID_LEN);
    memcpy(hdr.src_id, s_local_id, PKT_NODE_ID_LEN);
    memcpy(hdr.dst_id, dst_id, PKT_NODE_ID_LEN);

    uint8_t payload[1 + PCRYPTO_BUNDLE_MAX_LEN];
    payload[0] = subtype;
    if (extra_len) {
        memcpy(&payload[1], extra, extra_len);
    }

    uint8_t wire[PKT_MAX_WIRE_LEN];
    size_t wire_len = pkt_encode(wire, sizeof(wire), &hdr, payload, (uint8_t)(1 + extra_len));
    if (wire_len) {
        sx1262_send(wire, wire_len, 1000);
    }
}

static void send_key_bundle_request(const uint8_t dst_id[PKT_NODE_ID_LEN])
{
    send_key_bundle_frame(dst_id, KEY_BUNDLE_REQUEST, NULL, 0);
}

static void send_key_bundle_response(const uint8_t dst_id[PKT_NODE_ID_LEN])
{
    pcrypto_bundle_t bundle;
    if (pcrypto_local_bundle(&bundle) != ESP_OK) {
        ESP_LOGW(TAG, "brak wlasnego bundla kluczy do odeslania (prekeys wyczerpane?)");
        return;
    }
    send_key_bundle_frame(dst_id, KEY_BUNDLE_RESPONSE, bundle.data, bundle.len);
}

static void handle_key_bundle(const pkt_hdr_t *hdr, const uint8_t *payload)
{
    bool for_us = memcmp(hdr->dst_id, s_local_id, PKT_NODE_ID_LEN) == 0;
    if (!for_us || hdr->payload_len < 1) {
        return;
    }

    uint8_t subtype = payload[0];
    if (subtype == KEY_BUNDLE_REQUEST) {
        ESP_LOGI(TAG, "prosba o bundle kluczy, odsylam");
        send_key_bundle_response(hdr->src_id);
    } else if (subtype == KEY_BUNDLE_RESPONSE) {
        esp_err_t err = pcrypto_process_bundle(hdr->src_id, payload + 1, hdr->payload_len - 1);
        ESP_LOGI(TAG, "przetworzono bundle kluczy od sasiada, sesja %s",
                 err == ESP_OK ? "OK" : "NIEUDANA");
    }
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

    if (hdr.type == PKT_TYPE_KEY_BUNDLE) {
        // wymiana kluczy nie idzie przez dedup/flooding - zawsze tylko jeden skok,
        // patrz send_key_bundle_frame
        handle_key_bundle(&hdr, payload);
        return;
    }

    if (dedup_check_and_mark(hdr.msg_id)) {
        return; // ktos inny juz to rozgloscil, my tez to juz widzielismy
    }

    bool for_us = memcmp(hdr.dst_id, s_local_id, PKT_NODE_ID_LEN) == 0;
    if (for_us && hdr.type == PKT_TYPE_DATA && s_rx_cb) {
        uint8_t plaintext[PKT_MAX_PAYLOAD];
        size_t plaintext_len = 0;
        esp_err_t err = pcrypto_decrypt(hdr.src_id, payload, hdr.payload_len,
                                         plaintext, sizeof(plaintext), &plaintext_len);
        if (err == ESP_OK) {
            s_rx_cb(plaintext, plaintext_len, hdr.src_id);
        } else {
            ESP_LOGW(TAG, "nie udalo sie odszyfrowac wiadomosci od nadawcy (err=0x%x)", err);
        }
    }

    // Flooding: pakiet leci dalej niezaleznie czy byl dla nas - tak dziala epidemic
    // routing, ktos inny w zasiegu tez moze go potrzebowac. Zawsze surowe, nieodszyfrowane
    // bajty - posredni wezel nie ma i nie powinien miec mozliwosci poznania tresci
    // (model zero-trust, rozdz. 4.3 planu).
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

    esp_err_t crypto_err = pcrypto_init();
    if (crypto_err != ESP_OK) {
        return crypto_err;
    }

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

esp_err_t mesh_send(const uint8_t dst_id[PKT_NODE_ID_LEN], const uint8_t *plaintext, uint8_t len)
{
    if (len > PKT_MAX_PAYLOAD) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (!pcrypto_has_session(dst_id)) {
        // nie mamy jeszcze sesji Signal z tym peerem - nie mielibysmy jak zaszyfrowac,
        // wiec zamiast wiadomosci leci prosba o bundle kluczy. Wolajacy dostaje
        // ESP_ERR_NOT_FOUND i powinien sprobowac ponownie za chwile (patrz main.c)
        send_key_bundle_request(dst_id);
        return ESP_ERR_NOT_FOUND;
    }

    // narzut Double Ratchet / PreKeySignalMessage (klucze publiczne w pierwszej
    // wiadomosci sesji) - przy dlugich wiadomosciach na pierwszej wymianie moze
    // zabraknac miejsca w limicie payloadu. TODO: fragmentacja (rozdz. 5.3 planu),
    // na razie po prostu odrzucamy z bledem zamiast cos ucinac
    uint8_t ciphertext[PKT_MAX_PAYLOAD];
    size_t ciphertext_len = 0;
    esp_err_t enc_err = pcrypto_encrypt(dst_id, plaintext, len, ciphertext, sizeof(ciphertext), &ciphertext_len);
    if (enc_err != ESP_OK) {
        return enc_err;
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
    size_t wire_len = pkt_encode(wire, sizeof(wire), &hdr, ciphertext, (uint8_t)ciphertext_len);
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
