#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "pkt.h"

void presence_init(void);

void presence_touch(const uint8_t node_id[PKT_NODE_ID_LEN]);
bool presence_seen_recently(const uint8_t node_id[PKT_NODE_ID_LEN], uint32_t max_age_ms);

// true kiedy minal interval od ostatniego beacona (z jitterem) - mesh_poll() odpytuje
// to co jakis czas i wysyla nowy beacon kiedy przyjdzie pora
bool presence_beacon_due(void);
void presence_beacon_reset_timer(void);
