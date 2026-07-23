#pragma once

// pcrypto.c dolacza freertos/FreeRTOS.h przed semphr.h - w prawdziwym FreeRTOS to
// glowny naglowek konfiguracyjny, tutaj potrzebne tylko portMAX_DELAY

#include <stdint.h>

#define portMAX_DELAY 0xFFFFFFFFUL
