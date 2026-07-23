#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// Parametry LoRa dla jednej transmisji/odbioru. Domyślne wartości (patrz sx1262.c)
// to SF7/BW125/CR4:5 - rozsądny kompromis zasięg/przepustowość do testów w mieście,
// nie coś ostatecznego.
// Bandwidth - wartości zakodowane wg rejestrów SX1262, to NIE jest kHz wprost
#define SX1262_BW_125 0x04
#define SX1262_BW_250 0x05
#define SX1262_BW_500 0x06

#define SX1262_CR_4_5 0x01
#define SX1262_CR_4_6 0x02
#define SX1262_CR_4_7 0x03
#define SX1262_CR_4_8 0x04

typedef struct {
    uint32_t freq_hz;      // np. 868100000
    uint8_t  sf;           // spreading factor, 7-12
    uint8_t  bw;           // SX1262_BW_*
    uint8_t  cr;           // SX1262_CR_*
    int8_t   tx_power_dbm; // do +22, ale patrz limity ERP z rozdz. 3.1 planu zanim podniesiesz
} sx1262_config_t;

typedef void (*sx1262_rx_cb_t)(const uint8_t *buf, size_t len, int16_t rssi_dbm, int8_t snr_db);

esp_err_t sx1262_init(const sx1262_config_t *cfg);

// wysyła i blokuje do potwierdzenia TxDone albo timeoutu (ms)
esp_err_t sx1262_send(const uint8_t *buf, size_t len, uint32_t timeout_ms);

// włącza ciągły nasłuch, odebrane pakiety lecą do callbacku z kontekstu taska radiowego
esp_err_t sx1262_start_rx(sx1262_rx_cb_t cb);
