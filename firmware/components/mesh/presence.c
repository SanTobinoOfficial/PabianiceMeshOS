#include "presence.h"
#include <string.h>
#include "esp_timer.h"
#include "esp_random.h"

#define PRESENCE_TABLE_SIZE 32

typedef struct {
    uint8_t node_id[PKT_NODE_ID_LEN];
    int64_t last_seen_us;
    bool used;
} presence_entry_t;

static presence_entry_t s_table[PRESENCE_TABLE_SIZE];
static int64_t s_next_beacon_us;

// 60-120s wg rozdz. 5.4 - kompromis miedzy aktualnoscia tabeli obecnosci a zuzyciem
// baterii/pasma. Jitter losowy, zeby wezly wlaczone razem (np. cala paczka z jednego
// zamowienia) nie biły beaconem rowno w te sama sekunde i sie nie zagluszaly.
#define BEACON_INTERVAL_MIN_US (60LL * 1000000)
#define BEACON_INTERVAL_MAX_US (120LL * 1000000)

static int64_t next_beacon_delay_us(void)
{
    int64_t span = BEACON_INTERVAL_MAX_US - BEACON_INTERVAL_MIN_US;
    return BEACON_INTERVAL_MIN_US + (int64_t)(esp_random() % span);
}

void presence_init(void)
{
    memset(s_table, 0, sizeof(s_table));
    s_next_beacon_us = esp_timer_get_time() + next_beacon_delay_us();
}

void presence_touch(const uint8_t node_id[PKT_NODE_ID_LEN])
{
    int64_t now = esp_timer_get_time();
    int free_slot = -1;

    for (int i = 0; i < PRESENCE_TABLE_SIZE; i++) {
        if (s_table[i].used && memcmp(s_table[i].node_id, node_id, PKT_NODE_ID_LEN) == 0) {
            s_table[i].last_seen_us = now;
            return;
        }
        if (!s_table[i].used && free_slot < 0) {
            free_slot = i;
        }
    }

    // tabela pelna, a to nowy wezel - nadpisujemy najstarszy wpis. TODO: cos madrzejszego
    // niz liniowy skan jak lista realnie zacznie rosnac, na razie 32 sasiadow na raz
    // to i tak optymistyczne zalozenie dla wezla przy oknie
    if (free_slot < 0) {
        int oldest = 0;
        for (int i = 1; i < PRESENCE_TABLE_SIZE; i++) {
            if (s_table[i].last_seen_us < s_table[oldest].last_seen_us) {
                oldest = i;
            }
        }
        free_slot = oldest;
    }

    memcpy(s_table[free_slot].node_id, node_id, PKT_NODE_ID_LEN);
    s_table[free_slot].last_seen_us = now;
    s_table[free_slot].used = true;
}

bool presence_seen_recently(const uint8_t node_id[PKT_NODE_ID_LEN], uint32_t max_age_ms)
{
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < PRESENCE_TABLE_SIZE; i++) {
        if (s_table[i].used && memcmp(s_table[i].node_id, node_id, PKT_NODE_ID_LEN) == 0) {
            return (now - s_table[i].last_seen_us) <= ((int64_t)max_age_ms * 1000);
        }
    }
    return false;
}

bool presence_beacon_due(void)
{
    return esp_timer_get_time() >= s_next_beacon_us;
}

void presence_beacon_reset_timer(void)
{
    s_next_beacon_us = esp_timer_get_time() + next_beacon_delay_us();
}
