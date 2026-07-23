#pragma once

// pcrypto.c uzywa tylko esp_timer_get_time (mikrosekundy od bootu) do wypelnienia pola
// timestamp signed prekey - na hoscie liczymy od CLOCK_MONOTONIC (patrz shim.c)

#include <stdint.h>

int64_t esp_timer_get_time(void);
