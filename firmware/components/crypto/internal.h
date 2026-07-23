#pragma once

// wewnetrzny "klej" miedzy provider.c / store.c / pcrypto.c - nie wystawiamy tego
// na zewnatrz komponentu, stad nie w include/.

#include "nvs.h"
#include "signal_protocol.h"

nvs_handle_t crypto_nvs_handle(void);
signal_context *crypto_signal_ctx(void);
signal_protocol_store_context *crypto_store_ctx(void);

// NVS ogranicza klucze do 15 znakow, wiec 8-bajtowe ID wezla foldujemy xorem do 4
// bajtow zamiast pelnego hexa (16 znakow by sie nie zmiescilo z zadnym prefiksem)
void crypto_node_key(const uint8_t *node_id, const char *prefix, char out[16]);

// wypelnia signal_crypto_provider callbackami na mbedtls (provider.c)
void crypto_provider_fill(signal_crypto_provider *provider);

// rejestruje 4 store'y (session / pre_key / signed_pre_key / identity) w store_context
esp_err_t crypto_store_register(signal_protocol_store_context *store_ctx);
