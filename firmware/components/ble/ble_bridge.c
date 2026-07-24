// Serwer GATT (NimBLE) - jedna usluga custom, dwie charakterystyki: RX (telefon pisze
// [dst_id 8B][plaintext]) i TX (wezel notyfikuje [src_id 8B][plaintext]). Caly ciezar
// (routing mesh, X3DH, Double Ratchet) zostaje w mesh_send/mesh_set_rx_callback - ten
// plik tylko przeklada bajty miedzy BLE a tamtym API, nie zna sie na kryptografii.

#include "ble_bridge.h"

#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "mesh.h"
#include "pkt.h"

static const char *TAG = "ble_bridge";

// nazwa widoczna przy skanowaniu z telefonu - to pod nia szuka jej client/web-ble/
#define DEVICE_NAME "pabianice-node"

// losowe 128-bitowe UUID wygenerowane dla tego projektu - nie koliduja z niczym
// standardowym z rejestru Bluetooth SIG, bo to nie jest zarejestrowana usluga
#define SVC_UUID \
    0x2b, 0x19, 0x5c, 0xc7, 0x9a, 0x84, 0x4b, 0x1a, \
    0x8f, 0x6e, 0x11, 0x22, 0x33, 0x44, 0x00, 0x01
#define CHR_RX_UUID \
    0x2b, 0x19, 0x5c, 0xc7, 0x9a, 0x84, 0x4b, 0x1a, \
    0x8f, 0x6e, 0x11, 0x22, 0x33, 0x44, 0x00, 0x02
#define CHR_TX_UUID \
    0x2b, 0x19, 0x5c, 0xc7, 0x9a, 0x84, 0x4b, 0x1a, \
    0x8f, 0x6e, 0x11, 0x22, 0x33, 0x44, 0x00, 0x03

static uint16_t s_tx_val_handle;
static uint8_t s_own_addr_type;

// Kilku sasiadow (rodzina, ekipa przy jednym wezle) moze chciec korzystac z tego
// samego wezla naraz - trzymamy liste subskrybentow TX zamiast jednego conn_handle,
// zeby wiadomosc z mesh trafiala do wszystkich podlaczonych telefonow, nie tylko
// ostatniego ktory sie zasubskrybowal (patrz tez CONFIG_BT_NIMBLE_MAX_CONNECTIONS
// w sdkconfig.defaults i wznawianie advertisingu po udanym polaczeniu nizej)
#define BLE_MAX_SUBSCRIBERS 3
static uint16_t s_subscriber_conns[BLE_MAX_SUBSCRIBERS];
static int s_subscriber_count = 0;

static void subscriber_add(uint16_t conn_handle)
{
    for (int i = 0; i < s_subscriber_count; i++) {
        if (s_subscriber_conns[i] == conn_handle) {
            return; // juz na liscie (np. duplikat eventu SUBSCRIBE)
        }
    }
    if (s_subscriber_count < BLE_MAX_SUBSCRIBERS) {
        s_subscriber_conns[s_subscriber_count++] = conn_handle;
    }
}

static void subscriber_remove(uint16_t conn_handle)
{
    for (int i = 0; i < s_subscriber_count; i++) {
        if (s_subscriber_conns[i] == conn_handle) {
            s_subscriber_conns[i] = s_subscriber_conns[s_subscriber_count - 1];
            s_subscriber_count--;
            return;
        }
    }
}

static int ble_gap_event(struct ble_gap_event *event, void *arg);

static int gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY; // TX to notify-only, GAP/GATT nie powinien tu wywolac reada
    }

    uint8_t buf[PKT_NODE_ID_LEN + PKT_MAX_PAYLOAD];
    uint16_t len = 0;
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    if (len <= PKT_NODE_ID_LEN) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN; // brak tresci po dst_id
    }

    uint8_t dst_id[PKT_NODE_ID_LEN];
    memcpy(dst_id, buf, PKT_NODE_ID_LEN);
    esp_err_t err = mesh_send(dst_id, buf + PKT_NODE_ID_LEN, (uint8_t)(len - PKT_NODE_ID_LEN));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mesh_send z telefonu nie wyszlo: 0x%x", err);
    }
    return 0;
}

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(SVC_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // _ENC wymusza sparowane/zaszyfrowane polaczenie zanim ATT w ogole
                // przepusci zapis do gatt_svr_chr_access - patrz sekcja o bondingu
                // w ble_bridge_init() i firmware/components/ble/README.md
                .uuid = BLE_UUID128_DECLARE(CHR_RX_UUID),
                .access_cb = gatt_svr_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC,
            },
            {
                .uuid = BLE_UUID128_DECLARE(CHR_TX_UUID),
                .access_cb = gatt_svr_chr_access,
                .val_handle = &s_tx_val_handle,
                .flags = BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 }, // koniec listy charakterystyk
        },
    },
    { 0 }, // koniec listy uslug
};

static void ble_advertise(void)
{
    struct ble_hs_adv_fields fields = { 0 };
    struct ble_gap_adv_params adv_params = { 0 };
    const char *name = ble_svc_gap_device_name();

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_gap_adv_set_fields nie wyszlo: %d", rc);
        return;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_gap_adv_start nie wyszlo: %d", rc);
    }
}

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            ble_advertise(); // polaczenie nie wyszlo, wracamy do rozglaszania
            return 0;
        }
        // Wymuszamy parowanie/szyfrowanie od razu po polaczeniu (Just Works - ten
        // wezel nie ma ekranu ani klawiatury do wpisania PIN-u), zamiast czekac az
        // telefon sam o to poprosi przy pierwszym zapisie. Bez tego telefon moglby
        // np. zdazyc zasubskrybowac notyfikacje TX przed sparowaniem - BLE_GATT_CHR_F_WRITE_ENC
        // na RX i tak by zablokowal zapis, ale to i tak lepiej ustalic raz na wstepie
        {
            int rc = ble_gap_security_initiate(event->connect.conn_handle);
            if (rc != 0) {
                ESP_LOGW(TAG, "ble_gap_security_initiate nie wyszlo: %d", rc);
            }
        }
        // Wznawiamy advertising od razu, nie tylko po rozlaczeniu - inaczej po
        // pierwszym udanym polaczeniu wezel staje sie niewidoczny dla kolejnych
        // telefonow (NimBLE sam zatrzymuje advertising w momencie polaczenia).
        // Jesli limit CONFIG_BT_NIMBLE_MAX_CONNECTIONS jest juz wyczerpany,
        // ble_gap_adv_start po prostu nie wyjdzie - logujemy i nie traktujemy
        // tego jako bledu inicjalizacji
        ble_advertise();
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        subscriber_remove(event->disconnect.conn.conn_handle);
        ble_advertise();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        if (event->enc_change.status == 0) {
            ESP_LOGI(TAG, "polaczenie zaszyfrowane/sparowane (conn_handle=%d)",
                     event->enc_change.conn_handle);
        } else {
            ESP_LOGW(TAG, "parowanie/szyfrowanie nie wyszlo (status=%d) - zapis na RX i tak zostanie odrzucony",
                      event->enc_change.status);
        }
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_tx_val_handle) {
            if (event->subscribe.cur_notify) {
                subscriber_add(event->subscribe.conn_handle);
            } else {
                subscriber_remove(event->subscribe.conn_handle);
            }
            ESP_LOGI(TAG, "telefon %s notyfikacje TX (aktywnych subskrybentow: %d)",
                     event->subscribe.cur_notify ? "wlaczyl" : "wylaczyl", s_subscriber_count);
        }
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ble_advertise();
        return 0;

    default:
        return 0;
    }
}

static void ble_app_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr nie wyszlo: %d", rc);
        return;
    }
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto nie wyszlo: %d", rc);
        return;
    }
    ble_advertise();
}

static void ble_app_on_reset(int reason)
{
    ESP_LOGW(TAG, "nimble reset, powod: %d", reason);
}

static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_bridge_init(void)
{
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init nie wyszlo: 0x%x", ret);
        return ret;
    }

    ble_hs_cfg.sync_cb = ble_app_on_sync;
    ble_hs_cfg.reset_cb = ble_app_on_reset;

    // Bonding (parowanie Just Works - wezel nie ma ekranu/klawiatury na PIN, wiec
    // sm_mitm=0). Bez tego dowolny telefon w zasiegu mogl czytac/pisac na tej
    // usludze bez zadnego uwierzytelnienia - patrz README tego komponentu, sekcja
    // "Model zaufania". LE Secure Connections (sm_sc=1) zamiast przestarzalego
    // legacy pairing. Klucze bondingu wymieniamy w obie strony, zeby telefon tez
    // mogl zweryfikowac tozsamosc wezla przy kolejnych polaczeniach.
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // Trwaly magazyn kluczy bondingu - w przeciwienstwie do "golego" NimBLE (Apache
    // Mynewt), port ESP-IDF nie wymaga recznego ble_store_config_init(); NVS-owy
    // backend jest wpiety automatycznie przez sam CONFIG_BT_NIMBLE_NVS_PERSIST=y
    // (sdkconfig.defaults) - stad brak tu wywolania, ktore w ESP-IDF nawet nie istnieje.

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg nie wyszlo: %d", rc);
        return ESP_FAIL;
    }
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs nie wyszlo: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_svc_gap_device_name_set nie wyszlo: %d", rc);
        return ESP_FAIL;
    }

    // 8 (dst_id) + 200 (PKT_MAX_PAYLOAD) = 208B tresci + naglowek ATT - domyslne MTU
    // (23B) by tego nie udzwignelo, stad zadanie wiekszego zaraz po polaczeniu
    ble_att_set_preferred_mtu(247);

    nimble_port_freertos_init(ble_host_task);
    return ESP_OK;
}

void ble_bridge_on_mesh_rx(const uint8_t *payload, size_t len, const uint8_t src_id[PKT_NODE_ID_LEN])
{
    if (s_subscriber_count == 0) {
        return; // zaden telefon akurat nie subskrybuje - wiadomosc i tak zostaje w historii mesh
    }
    if (len > PKT_MAX_PAYLOAD) {
        len = PKT_MAX_PAYLOAD; // nie powinno sie zdarzyc, payload z mesh juz jest ograniczony
    }

    uint8_t buf[PKT_NODE_ID_LEN + PKT_MAX_PAYLOAD];
    memcpy(buf, src_id, PKT_NODE_ID_LEN);
    memcpy(buf + PKT_NODE_ID_LEN, payload, len);

    // kazde wywolanie ble_gatts_notify_custom przejmuje/zwalnia swoj mbuf, wiec
    // budujemy osobny per subskrybent zamiast probowac dzielic jeden
    for (int i = 0; i < s_subscriber_count; i++) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, PKT_NODE_ID_LEN + len);
        if (!om) {
            ESP_LOGW(TAG, "ble_hs_mbuf_from_flat nie wyszlo, gubie notyfikacje dla jednego subskrybenta");
            continue;
        }
        int rc = ble_gatts_notify_custom(s_subscriber_conns[i], s_tx_val_handle, om);
        if (rc != 0) {
            ESP_LOGW(TAG, "ble_gatts_notify_custom nie wyszlo: %d", rc);
        }
    }
}
