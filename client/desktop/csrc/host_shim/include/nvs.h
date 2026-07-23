#pragma once

// zamiennik nvs.h - store.c/pcrypto.c uzywaja tylko blob get/set/erase, u32 get/set,
// open i commit. Na hoscie backend to zwykle pliki w katalogu ustawionym przez
// host_shim_set_data_dir() (patrz host_shim.h), jeden plik na klucz - patrz shim.c

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef int nvs_handle_t;

typedef enum {
    NVS_READONLY,
    NVS_READWRITE
} nvs_open_mode_t;

esp_err_t nvs_open(const char *name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle);
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length);
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length);
esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key);
esp_err_t nvs_commit(nvs_handle_t handle);
esp_err_t nvs_get_u32(nvs_handle_t handle, const char *key, uint32_t *out_value);
esp_err_t nvs_set_u32(nvs_handle_t handle, const char *key, uint32_t value);
