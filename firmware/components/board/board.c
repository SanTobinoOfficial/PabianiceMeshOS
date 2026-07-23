#include "board.h"
#include "esp_log.h"

static const char *TAG = "board";
static spi_device_handle_t s_radio_spi = NULL;

esp_err_t board_init(void)
{
    gpio_config_t reset_cfg = {
        .pin_bit_mask = 1ULL << BOARD_PIN_RADIO_RESET,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&reset_cfg);
    gpio_set_level(BOARD_PIN_RADIO_RESET, 1);

    gpio_config_t busy_cfg = {
        .pin_bit_mask = 1ULL << BOARD_PIN_RADIO_BUSY,
        .mode = GPIO_MODE_INPUT,
    };
    gpio_config(&busy_cfg);

    // DIO1 budzi nas na TxDone/RxDone/Timeout, patrz radio/sx1262.c
    gpio_config_t dio1_cfg = {
        .pin_bit_mask = 1ULL << BOARD_PIN_RADIO_DIO1,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&dio1_cfg);

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BOARD_PIN_MOSI,
        .miso_io_num = BOARD_PIN_MISO,
        .sclk_io_num = BOARD_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256, // tyle ile ma FIFO w SX1262, i tak więcej na raz nie wyślemy
    };
    esp_err_t err = spi_bus_initialize(BOARD_RADIO_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: 0x%x", err);
        return err;
    }

    spi_device_interface_config_t dev_cfg = {
        // datasheet dopuszcza do 16MHz, ale na krótkich przewodach do modułu na goldpinach
        // 8MHz daje spokojny margines - jak ktoś ma płytkę PCB może spróbować podbić
        .clock_speed_hz = 8 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = BOARD_PIN_RADIO_NSS,
        .queue_size = 4,
    };
    return spi_bus_add_device(BOARD_RADIO_SPI_HOST, &dev_cfg, &s_radio_spi);
}

spi_device_handle_t board_radio_spi(void)
{
    return s_radio_spi;
}
