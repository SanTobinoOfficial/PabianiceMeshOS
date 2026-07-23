#pragma once

// Opcody komend SX1262, wg datasheet Semtech (rozdz. 13, "Commands Selection").
// Nazwy 1:1 z datasheetem żeby łatwo było sobie sprawdzić w PDFie co robi który.

#define SX1262_OP_SET_SLEEP              0x84
#define SX1262_OP_SET_STANDBY            0x80
#define SX1262_OP_SET_FS                 0xC1
#define SX1262_OP_SET_TX                 0x83
#define SX1262_OP_SET_RX                 0x82
#define SX1262_OP_SET_RX_DUTY_CYCLE      0x94
#define SX1262_OP_SET_CAD                0xC5
#define SX1262_OP_SET_REGULATOR_MODE     0x96
#define SX1262_OP_CALIBRATE              0x89
#define SX1262_OP_CALIBRATE_IMAGE        0x98
#define SX1262_OP_SET_PA_CONFIG          0x95
#define SX1262_OP_SET_RX_TX_FALLBACK     0x93

#define SX1262_OP_WRITE_REGISTER         0x0D
#define SX1262_OP_READ_REGISTER          0x1D
#define SX1262_OP_WRITE_BUFFER           0x0E
#define SX1262_OP_READ_BUFFER            0x1E

#define SX1262_OP_SET_DIO_IRQ_PARAMS     0x08
#define SX1262_OP_GET_IRQ_STATUS         0x12
#define SX1262_OP_CLEAR_IRQ_STATUS       0x02
#define SX1262_OP_SET_DIO2_AS_RF_SWITCH  0x9D
#define SX1262_OP_SET_DIO3_AS_TCXO_CTRL  0x97

#define SX1262_OP_SET_RF_FREQUENCY       0x86
#define SX1262_OP_SET_PACKET_TYPE        0x8A
#define SX1262_OP_GET_PACKET_TYPE        0x11
#define SX1262_OP_SET_TX_PARAMS          0x8E
#define SX1262_OP_SET_MODULATION_PARAMS  0x8B
#define SX1262_OP_SET_PACKET_PARAMS      0x8C
#define SX1262_OP_SET_BUFFER_BASE_ADDR   0x8F
#define SX1262_OP_SET_LORA_SYMB_TIMEOUT  0xA0

#define SX1262_OP_GET_STATUS             0xC0
#define SX1262_OP_GET_RX_BUFFER_STATUS   0x13
#define SX1262_OP_GET_PACKET_STATUS      0x14
#define SX1262_OP_GET_DEVICE_ERRORS      0x17
#define SX1262_OP_CLEAR_DEVICE_ERRORS    0x07

// SetPacketType
#define SX1262_PACKET_TYPE_LORA          0x01

// IRQ bity (16-bitowa maska, SetDioIrqParams / GetIrqStatus)
#define SX1262_IRQ_TX_DONE            (1 << 0)
#define SX1262_IRQ_RX_DONE            (1 << 1)
#define SX1262_IRQ_PREAMBLE_DETECTED  (1 << 2)
#define SX1262_IRQ_SYNC_WORD_VALID    (1 << 3)
#define SX1262_IRQ_HEADER_VALID       (1 << 4)
#define SX1262_IRQ_HEADER_ERR         (1 << 5)
#define SX1262_IRQ_CRC_ERR            (1 << 6)
#define SX1262_IRQ_CAD_DONE           (1 << 7)
#define SX1262_IRQ_CAD_DETECTED       (1 << 8)
#define SX1262_IRQ_TIMEOUT            (1 << 9)
#define SX1262_IRQ_ALL                0x03FF

// register do korekty sensitivity przy BW=500kHz, patrz erratum 15.1 w datasheet -
// bez tego RX na 500kHz jest wyraźnie gorszy, my i tak jedziemy na 125kHz na start
// ale zostawiam żeby nie zapomnieć jak kiedyś ktoś podniesie bandwidth
#define SX1262_REG_TX_MODULATION       0x0889
