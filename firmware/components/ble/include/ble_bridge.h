#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

// Most BLE (NimBLE GATT server) miedzy telefonem bez internetu a siecia mesh. Telefon
// laczy sie po BLE z jednym, najblizszym wezlem - ten wezel robi za niego cala reszte
// (routing, X3DH, Double Ratchet, patrz components/mesh i components/crypto). Zero
// internetu/serwera potrzebne do samej rozmowy w zasiegu radiowym LoRa. Referencyjny
// klient przegladarkowy (Web Bluetooth) w client/web-ble/.

// startuje serwer GATT + rozglaszanie. Wywolaj raz z app_main, po mesh_init()
esp_err_t ble_bridge_init(void);

// wolaj z wlasnego callbacku mesh_set_rx_callback (patrz main.c) - przekazuje
// odszyfrowana wiadomosc dalej do podlaczonego telefonu (notify), no-op jesli nikt
// akurat nie subskrybuje charakterystyki TX
void ble_bridge_on_mesh_rx(const uint8_t *payload, size_t len, const uint8_t src_id[8]);
