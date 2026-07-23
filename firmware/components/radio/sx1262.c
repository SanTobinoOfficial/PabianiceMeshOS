#include <string.h>
#include <stdbool.h>
#include "sx1262.h"
#include "sx1262_regs.h"
#include "board.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "sx1262";

static sx1262_rx_cb_t s_rx_cb = NULL;
static bool s_rx_enabled = false;
static SemaphoreHandle_t s_tx_done_sem;
static TaskHandle_t s_irq_task_handle;

static void wait_busy(void)
{
    // BUSY schodzi max ~3.5ms po resecie, a przy zwykłych komendach to raczej
    // pojedyncze mikrosekundy - pollowanie zamiast przerwania, bo i tak szybciej
    int guard = 0;
    while (gpio_get_level(BOARD_PIN_RADIO_BUSY)) {
        esp_rom_delay_us(10);
        if (++guard > 100000) { // ~1s, coś poszło nie tak (zły montaż / martwy moduł)
            ESP_LOGE(TAG, "BUSY nie schodzi, sprawdz okablowanie");
            break;
        }
    }
}

static void sx1262_write(uint8_t opcode, const uint8_t *data, size_t len)
{
    wait_busy();
    uint8_t tx[1 + len];
    tx[0] = opcode;
    if (len) memcpy(&tx[1], data, len);

    spi_transaction_t t = {
        .length = 8 * (1 + len),
        .tx_buffer = tx,
    };
    spi_device_transmit(board_radio_spi(), &t);
    wait_busy();
}

// komendy odczytu mają dodatkowy bajt NOP po opkodzie/adresie zanim zaczną
// przychodzić właściwe dane - chip w tym czasie donosi status
static void sx1262_read(uint8_t opcode, const uint8_t *addr, size_t addr_len,
                         uint8_t *out, size_t out_len)
{
    wait_busy();
    size_t total = 1 + addr_len + 1 + out_len;
    uint8_t tx[total];
    uint8_t rx[total];
    memset(tx, 0, total);
    tx[0] = opcode;
    if (addr_len) memcpy(&tx[1], addr, addr_len);

    spi_transaction_t t = {
        .length = 8 * total,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    spi_device_transmit(board_radio_spi(), &t);
    memcpy(out, &rx[1 + addr_len + 1], out_len);
    wait_busy();
}

static void set_dio_irq_params(uint16_t irq_mask, uint16_t dio1_mask)
{
    uint8_t p[8] = {
        irq_mask >> 8, irq_mask & 0xFF,
        dio1_mask >> 8, dio1_mask & 0xFF,
        0, 0, // DIO2 - niewykorzystywany, przełączanie RX/TX anteny robi sam moduł
        0, 0, // DIO3
    };
    sx1262_write(SX1262_OP_SET_DIO_IRQ_PARAMS, p, sizeof(p));
}

static void set_rf_frequency(uint32_t freq_hz)
{
    // wzór z datasheet: freq_reg = freq_hz * 2^25 / F_XTAL, F_XTAL = 32MHz na tym module
    uint32_t reg = (uint32_t)(((uint64_t)freq_hz << 25) / 32000000ULL);
    uint8_t p[4] = { (reg >> 24) & 0xFF, (reg >> 16) & 0xFF, (reg >> 8) & 0xFF, reg & 0xFF };
    sx1262_write(SX1262_OP_SET_RF_FREQUENCY, p, sizeof(p));
}

static void set_modulation_params(uint8_t sf, uint8_t bw, uint8_t cr)
{
    // LowDataRateOptimize - włączamy dla wysokich SF na wąskim BW, patrz AN1200.22,
    // bez tego przy SF11/12 na 125kHz robi się niestabilnie (symbol > 16ms)
    uint8_t ldro = (sf >= 11 && bw == SX1262_BW_125) ? 1 : 0;
    uint8_t p[4] = { sf, bw, cr, ldro };
    sx1262_write(SX1262_OP_SET_MODULATION_PARAMS, p, sizeof(p));
}

static void set_packet_params(uint8_t payload_len)
{
    uint16_t preamble = 8;
    uint8_t p[6] = {
        preamble >> 8, preamble & 0xFF,
        0x00,        // header type: explicit - nadawca sam wkleja realną długość w nagłówku LoRa
        payload_len,
        0x01,        // CRC on
        0x00,        // standard IQ
    };
    sx1262_write(SX1262_OP_SET_PACKET_PARAMS, p, sizeof(p));
}

static void enter_continuous_rx(void)
{
    set_packet_params(255); // w RX i tak realną długość dyktuje nagłówek odebranego pakietu
    uint8_t timeout[3] = { 0xFF, 0xFF, 0xFF }; // continuous
    sx1262_write(SX1262_OP_SET_RX, timeout, sizeof(timeout));
}

static void handle_irq(void)
{
    uint8_t irq_raw[2];
    sx1262_read(SX1262_OP_GET_IRQ_STATUS, NULL, 0, irq_raw, sizeof(irq_raw));
    uint16_t irq = (irq_raw[0] << 8) | irq_raw[1];

    uint8_t clear[2] = { irq_raw[0], irq_raw[1] };
    sx1262_write(SX1262_OP_CLEAR_IRQ_STATUS, clear, sizeof(clear));

    if (irq & SX1262_IRQ_TX_DONE) {
        xSemaphoreGive(s_tx_done_sem);
        if (s_rx_enabled) {
            enter_continuous_rx();
        }
    }

    if (irq & SX1262_IRQ_RX_DONE) {
        if (irq & SX1262_IRQ_CRC_ERR) {
            ESP_LOGW(TAG, "pakiet z bledem CRC, odrzucam");
            return;
        }
        uint8_t buf_status[2];
        sx1262_read(SX1262_OP_GET_RX_BUFFER_STATUS, NULL, 0, buf_status, sizeof(buf_status));
        uint8_t payload_len = buf_status[0];
        uint8_t start_offset = buf_status[1];

        uint8_t rx_buf[256];
        uint8_t addr = start_offset;
        sx1262_read(SX1262_OP_READ_BUFFER, &addr, 1, rx_buf, payload_len);

        uint8_t status3[3];
        sx1262_read(SX1262_OP_GET_PACKET_STATUS, NULL, 0, status3, sizeof(status3));
        // wg datasheet: rssi_pkt = -status3[0]/2 [dBm], snr_pkt = (int8_t)status3[1] / 4 [dB]
        int16_t rssi = -(status3[0] / 2);
        int8_t snr = ((int8_t)status3[1]) / 4;

        if (s_rx_cb) {
            s_rx_cb(rx_buf, payload_len, rssi, snr);
        }
        // moduł w trybie continuous rx sam wraca do nasłuchu, nic więcej nie trzeba
    }
}

static void irq_task(void *arg)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        handle_irq();
    }
}

static void IRAM_ATTR dio1_isr(void *arg)
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_irq_task_handle, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

esp_err_t sx1262_init(const sx1262_config_t *cfg)
{
    s_tx_done_sem = xSemaphoreCreateBinary();
    xTaskCreate(irq_task, "sx1262_irq", 4096, NULL, configMAX_PRIORITIES - 2, &s_irq_task_handle);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BOARD_PIN_RADIO_DIO1, dio1_isr, NULL);

    // reset modułu - min 100us nisko wg datasheet, dajemy z zapasem
    gpio_set_level(BOARD_PIN_RADIO_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(BOARD_PIN_RADIO_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    wait_busy();

    uint8_t standby_rc = 0x00;
    sx1262_write(SX1262_OP_SET_STANDBY, &standby_rc, 1);

    uint8_t dcdc = 0x01; // ten moduł ma DC-DC, nie LDO - patrz nota aplikacyjna producenta
    sx1262_write(SX1262_OP_SET_REGULATOR_MODE, &dcdc, 1);

    uint8_t calib_all = 0x7F;
    sx1262_write(SX1262_OP_CALIBRATE, &calib_all, 1);
    vTaskDelay(pdMS_TO_TICKS(5));

    uint8_t packet_type = SX1262_PACKET_TYPE_LORA;
    sx1262_write(SX1262_OP_SET_PACKET_TYPE, &packet_type, 1);

    set_rf_frequency(cfg->freq_hz);
    set_modulation_params(cfg->sf, cfg->bw, cfg->cr);
    set_packet_params(255);

    // PA config dla SX1262 (nie SX1261!) na max moc, realną moc nadania i tak
    // przycina SetTxParams - patrz datasheet tabela 13-21
    uint8_t pa_cfg[4] = { 0x04, 0x07, 0x00, 0x01 };
    sx1262_write(SX1262_OP_SET_PA_CONFIG, pa_cfg, sizeof(pa_cfg));

    uint8_t tx_params[2] = { (uint8_t)cfg->tx_power_dbm, 0x04 }; // ramp 200us
    sx1262_write(SX1262_OP_SET_TX_PARAMS, tx_params, sizeof(tx_params));

    uint8_t buf_base[2] = { 0x00, 0x00 };
    sx1262_write(SX1262_OP_SET_BUFFER_BASE_ADDR, buf_base, sizeof(buf_base));

    set_dio_irq_params(SX1262_IRQ_TX_DONE | SX1262_IRQ_RX_DONE | SX1262_IRQ_CRC_ERR,
                        SX1262_IRQ_TX_DONE | SX1262_IRQ_RX_DONE | SX1262_IRQ_CRC_ERR);

    ESP_LOGI(TAG, "SX1262 gotowy: %lu Hz, SF%d", (unsigned long)cfg->freq_hz, cfg->sf);
    return ESP_OK;
}

esp_err_t sx1262_send(const uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    if (len > 255) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t addr = 0x00;
    uint8_t wbuf[1 + len];
    wbuf[0] = addr;
    memcpy(&wbuf[1], buf, len);
    sx1262_write(SX1262_OP_WRITE_BUFFER, wbuf, sizeof(wbuf));

    set_packet_params((uint8_t)len);

    xSemaphoreTake(s_tx_done_sem, 0); // wyczyść ewentualny stary sygnał

    uint8_t tx_timeout[3] = { 0x00, 0x00, 0x00 }; // 0 = bez limitu, czekamy do faktycznego TxDone
    sx1262_write(SX1262_OP_SET_TX, tx_timeout, sizeof(tx_timeout));

    if (xSemaphoreTake(s_tx_done_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        ESP_LOGW(TAG, "timeout czekania na TxDone");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t sx1262_start_rx(sx1262_rx_cb_t cb)
{
    s_rx_cb = cb;
    s_rx_enabled = true;
    enter_continuous_rx();
    return ESP_OK;
}
