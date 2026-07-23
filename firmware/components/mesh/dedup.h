#pragma once

#include <stdbool.h>
#include "pkt.h"

void dedup_init(void);

// true = to msg_id juz widzielismy (drop). false = nowe, i od teraz juz zapamietane.
bool dedup_check_and_mark(const uint8_t msg_id[PKT_MSG_ID_LEN]);
