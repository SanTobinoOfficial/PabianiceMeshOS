#pragma once

#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

// Okablowanie pod ESP32-S3-DevKitC-1 + moduł SX1262 (testowane na Ebyte E22-900M22S
// wpiętym na goldpiny). Inny board / inna płytka -> po prostu podmień te definicje,
// nic poza board.c od tego nie zależy.
#define BOARD_RADIO_SPI_HOST   SPI2_HOST
#define BOARD_PIN_MOSI         GPIO_NUM_11
#define BOARD_PIN_MISO         GPIO_NUM_13
#define BOARD_PIN_SCLK         GPIO_NUM_12
#define BOARD_PIN_RADIO_NSS    GPIO_NUM_10
#define BOARD_PIN_RADIO_RESET  GPIO_NUM_9
#define BOARD_PIN_RADIO_BUSY   GPIO_NUM_8
#define BOARD_PIN_RADIO_DIO1   GPIO_NUM_7

esp_err_t board_init(void);
spi_device_handle_t board_radio_spi(void);
