#include "dedup.h"
#include <string.h>

// 64 ostatnich ID starcza z zapasem przy TTL=8 i ruchu jaki realistycznie generuje
// kilkanascie-kilkadziesiat wezlow w zasiegu - jak kiedys zabraknie, to i tak wczesniej
// zabraknie czasu antenowego niz miejsca w tej tablicy
#define DEDUP_CACHE_SIZE 64

typedef struct {
    uint8_t msg_id[PKT_MSG_ID_LEN];
    bool used;
} dedup_entry_t;

static dedup_entry_t s_cache[DEDUP_CACHE_SIZE];
static int s_next_slot;

void dedup_init(void)
{
    memset(s_cache, 0, sizeof(s_cache));
    s_next_slot = 0;
}

bool dedup_check_and_mark(const uint8_t msg_id[PKT_MSG_ID_LEN])
{
    for (int i = 0; i < DEDUP_CACHE_SIZE; i++) {
        if (s_cache[i].used && memcmp(s_cache[i].msg_id, msg_id, PKT_MSG_ID_LEN) == 0) {
            return true;
        }
    }

    // ring buffer - najstarszy wpis po prostu wypada, nie trzymamy timestampow
    memcpy(s_cache[s_next_slot].msg_id, msg_id, PKT_MSG_ID_LEN);
    s_cache[s_next_slot].used = true;
    s_next_slot = (s_next_slot + 1) % DEDUP_CACHE_SIZE;
    return false;
}
