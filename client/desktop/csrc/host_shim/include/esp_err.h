#pragma once

// Podmienia esp_err.h z ESP-IDF na tyle, ile potrzebuje firmware/components/crypto/*.c
// zeby skompilowac sie na hoscie bez zmiany ani jednej linii tamtego kodu - patrz
// client/desktop/README.md, sekcja o tym dlaczego dzielimy caly plik z firmware.

typedef int esp_err_t;

#define ESP_OK               0
#define ESP_FAIL            -1
#define ESP_ERR_NO_MEM        0x101
#define ESP_ERR_INVALID_ARG   0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE  0x104
#define ESP_ERR_NOT_FOUND     0x105
