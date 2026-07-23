#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

// Owija libsignal-protocol-c (X3DH + Double Ratchet) do postaci wygodnej dla mesh.c.
// Nie zalezy od formatu pakietu z komponentu mesh - stad wlasna stala dlugosci ID,
// zamiast brac PKT_NODE_ID_LEN z pkt.h (inaczej mesh<->crypto zapetla sie zaleznosciami
// w CMake). Musi sie zgadzac z PKT_NODE_ID_LEN w components/mesh/include/pkt.h - obecnie 8.
#define PCRYPTO_PEER_ID_LEN 8

// tyle miejsca zostawiamy na bundle kluczy w jednym pakiecie KEY_BUNDLE (rozdz. 5.3/6.2
// planu - limit payloadu to 200B, -1 bajt na subtype w mesh.c)
#define PCRYPTO_BUNDLE_MAX_LEN 199

typedef struct {
    uint8_t data[PCRYPTO_BUNDLE_MAX_LEN];
    size_t len;
} pcrypto_bundle_t;

// generuje/laduje z NVS tozsamosc (identity key + prekeys), przygotowuje libsignal.
// wywolaj raz po nvs_flash_init(), przed czymkolwiek innym w tym module.
esp_err_t pcrypto_init(void);

// nasz komplet kluczy publicznych do rozeslania peerowi, zeby mogl zaczac sesje X3DH
esp_err_t pcrypto_local_bundle(pcrypto_bundle_t *out);

// przetwarza bundle otrzymany od peera - zaklada sesje Double Ratchet (X3DH)
esp_err_t pcrypto_process_bundle(const uint8_t peer_id[PCRYPTO_PEER_ID_LEN],
                                  const uint8_t *bundle_data, size_t bundle_len);

// mamy juz ustanowiona sesje z tym peerem?
bool pcrypto_has_session(const uint8_t peer_id[PCRYPTO_PEER_ID_LEN]);

// szyfruje wiadomosc do peera. Bez sesji zwraca ESP_ERR_INVALID_STATE - trzeba
// najpierw wymienic bundle (patrz mesh.c, PKT_TYPE_KEY_BUNDLE).
esp_err_t pcrypto_encrypt(const uint8_t peer_id[PCRYPTO_PEER_ID_LEN],
                          const uint8_t *plaintext, size_t plaintext_len,
                          uint8_t *out, size_t out_cap, size_t *out_len);

// odszyfrowuje wiadomosc od peera - sam rozpoznaje pierwsza wiadomosc sesji
// (PreKeySignalMessage) od kolejnych (SignalMessage) po bajcie typu doklejonym
// w pcrypto_encrypt.
esp_err_t pcrypto_decrypt(const uint8_t peer_id[PCRYPTO_PEER_ID_LEN],
                          const uint8_t *ciphertext, size_t ciphertext_len,
                          uint8_t *out, size_t out_cap, size_t *out_len);
