#pragma once

// zamiennik esp_random.h - provider.c wola tylko esp_fill_random, reszte (HW RNG na
// ESP32) implementuje shim.c przez /dev/urandom (jadro linuksa, wystarczajaco dobre
// zrodlo entropii na hoscie, nie trzeba wlasnego DRBG)

#include <stddef.h>
#include <stdint.h>

void esp_fill_random(uint8_t *buf, size_t len);
