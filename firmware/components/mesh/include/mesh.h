#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "pkt.h"

typedef void (*mesh_rx_cb_t)(const uint8_t *payload, size_t len,
                              const uint8_t src_id[PKT_NODE_ID_LEN]);

// odpala radio + routing, zaczyna nasluchiwac
esp_err_t mesh_init(void);

// wysyla nowa wiadomosc do sieci, TTL startowy = PKT_TTL_DEFAULT
esp_err_t mesh_send(const uint8_t dst_id[PKT_NODE_ID_LEN], const uint8_t *payload, uint8_t len);

void mesh_set_rx_callback(mesh_rx_cb_t cb);

// wolaj regularnie z glownej petli - obsluguje timer beacona obecnosci
void mesh_poll(void);

// nasze ID w sieci (8 bajtow) - na razie pochodna adresu MAC, docelowo hash klucza publicznego
const uint8_t *mesh_local_id(void);
