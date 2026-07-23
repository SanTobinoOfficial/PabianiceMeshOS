// signal_protocol_store_context callbacki - trzymamy wszystko w NVS. Docelowo (rozdz. 7.4
// planu, sekcja o module bezpiecznym) klucz tozsamosci powinien siedziec w ATECC608A i nigdy
// nie wychodzic w postaci jawnej - to jednak wymaga sprzetu ktorego jeszcze na tym etapie
// nie mamy zamontowanego, wiec na razie NVS (flash), co najmniej podpisany/szyfrowany przez
// esp-idf "NVS encryption" jak sie skonfiguruje partycje - TODO zanim to trafi do realnych rak.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "internal.h"

void crypto_node_key(const uint8_t *node_id, const char *prefix, char out[16])
{
    // NVS ogranicza klucze do 15 znakow, wiec pelne 8 bajtow ID jako hex (16 znakow)
    // sie nie zmiesci z zadnym prefiksem - foldujemy xorem do 4 bajtow. Kolizje przy
    // duzej liczbie wezlow sa teoretycznie mozliwe, w skali prototypu to nie problem
    uint8_t folded[4];
    for (int i = 0; i < 4; i++) {
        folded[i] = node_id[i] ^ node_id[i + 4];
    }
    snprintf(out, 16, "%s%02x%02x%02x%02x", prefix, folded[0], folded[1], folded[2], folded[3]);
}

static int load_session_func(signal_buffer **record, signal_buffer **user_record,
                              const signal_protocol_address *address, void *user_data)
{
    char key[16];
    crypto_node_key((const uint8_t *)address->name, "ss_", key);

    size_t len = 0;
    if (nvs_get_blob(crypto_nvs_handle(), key, NULL, &len) != ESP_OK || len == 0) {
        return 0;
    }
    uint8_t *buf = malloc(len);
    if (!buf) {
        return SG_ERR_NOMEM;
    }
    nvs_get_blob(crypto_nvs_handle(), key, buf, &len);
    *record = signal_buffer_create(buf, len);
    free(buf);
    return *record ? 1 : SG_ERR_NOMEM;
}

static int get_sub_device_sessions_func(signal_int_list **sessions, const char *name, size_t name_len, void *user_data)
{
    // brak wsparcia multi-device na tym etapie - kazdy wezel to zawsze device_id=1,
    // wiec i tak zawsze jest co najwyzej jedna sesja do zwrocenia
    (void)name_len;
    signal_int_list *list = signal_int_list_alloc();
    if (!list) {
        return SG_ERR_NOMEM;
    }
    char key[16];
    crypto_node_key((const uint8_t *)name, "ss_", key);
    size_t len = 0;
    if (nvs_get_blob(crypto_nvs_handle(), key, NULL, &len) == ESP_OK) {
        signal_int_list_push_back(list, 1);
    }
    *sessions = list;
    return (int)signal_int_list_size(list);
}

static int store_session_func(const signal_protocol_address *address, uint8_t *record, size_t record_len,
                               uint8_t *user_record, size_t user_record_len, void *user_data)
{
    char key[16];
    crypto_node_key((const uint8_t *)address->name, "ss_", key);
    if (nvs_set_blob(crypto_nvs_handle(), key, record, record_len) != ESP_OK) {
        return SG_ERR_UNKNOWN;
    }
    nvs_commit(crypto_nvs_handle());
    return 0;
}

static int contains_session_func(const signal_protocol_address *address, void *user_data)
{
    char key[16];
    crypto_node_key((const uint8_t *)address->name, "ss_", key);
    size_t len = 0;
    return nvs_get_blob(crypto_nvs_handle(), key, NULL, &len) == ESP_OK ? 1 : 0;
}

static int delete_session_func(const signal_protocol_address *address, void *user_data)
{
    char key[16];
    crypto_node_key((const uint8_t *)address->name, "ss_", key);
    int ret = (nvs_erase_key(crypto_nvs_handle(), key) == ESP_OK) ? 1 : 0;
    nvs_commit(crypto_nvs_handle());
    return ret;
}

static int delete_all_sessions_func(const char *name, size_t name_len, void *user_data)
{
    char key[16];
    crypto_node_key((const uint8_t *)name, "ss_", key);
    int ret = (nvs_erase_key(crypto_nvs_handle(), key) == ESP_OK) ? 1 : 0;
    nvs_commit(crypto_nvs_handle());
    return ret;
}

// --- pre_key_store: kluczowane bezposrednio numerycznym ID, nie ID wezla ---

static int load_pre_key(signal_buffer **record, uint32_t pre_key_id, void *user_data)
{
    char key[16];
    snprintf(key, sizeof(key), "pk_%u", (unsigned)pre_key_id);
    size_t len = 0;
    if (nvs_get_blob(crypto_nvs_handle(), key, NULL, &len) != ESP_OK) {
        return SG_ERR_INVALID_KEY_ID;
    }
    uint8_t *buf = malloc(len);
    if (!buf) {
        return SG_ERR_NOMEM;
    }
    nvs_get_blob(crypto_nvs_handle(), key, buf, &len);
    *record = signal_buffer_create(buf, len);
    free(buf);
    return *record ? SG_SUCCESS : SG_ERR_NOMEM;
}

static int store_pre_key(uint32_t pre_key_id, uint8_t *record, size_t record_len, void *user_data)
{
    char key[16];
    snprintf(key, sizeof(key), "pk_%u", (unsigned)pre_key_id);
    if (nvs_set_blob(crypto_nvs_handle(), key, record, record_len) != ESP_OK) {
        return SG_ERR_UNKNOWN;
    }
    nvs_commit(crypto_nvs_handle());
    return 0;
}

static int contains_pre_key(uint32_t pre_key_id, void *user_data)
{
    char key[16];
    snprintf(key, sizeof(key), "pk_%u", (unsigned)pre_key_id);
    size_t len = 0;
    return nvs_get_blob(crypto_nvs_handle(), key, NULL, &len) == ESP_OK ? 1 : 0;
}

static int remove_pre_key(uint32_t pre_key_id, void *user_data)
{
    char key[16];
    snprintf(key, sizeof(key), "pk_%u", (unsigned)pre_key_id);
    esp_err_t err = nvs_erase_key(crypto_nvs_handle(), key);
    nvs_commit(crypto_nvs_handle());
    return err == ESP_OK ? 0 : SG_ERR_UNKNOWN;
}

// --- signed_pre_key_store ---

static int load_signed_pre_key(signal_buffer **record, uint32_t signed_pre_key_id, void *user_data)
{
    char key[16];
    snprintf(key, sizeof(key), "spk_%u", (unsigned)signed_pre_key_id);
    size_t len = 0;
    if (nvs_get_blob(crypto_nvs_handle(), key, NULL, &len) != ESP_OK) {
        return SG_ERR_INVALID_KEY_ID;
    }
    uint8_t *buf = malloc(len);
    if (!buf) {
        return SG_ERR_NOMEM;
    }
    nvs_get_blob(crypto_nvs_handle(), key, buf, &len);
    *record = signal_buffer_create(buf, len);
    free(buf);
    return *record ? SG_SUCCESS : SG_ERR_NOMEM;
}

static int store_signed_pre_key(uint32_t signed_pre_key_id, uint8_t *record, size_t record_len, void *user_data)
{
    char key[16];
    snprintf(key, sizeof(key), "spk_%u", (unsigned)signed_pre_key_id);
    if (nvs_set_blob(crypto_nvs_handle(), key, record, record_len) != ESP_OK) {
        return SG_ERR_UNKNOWN;
    }
    nvs_commit(crypto_nvs_handle());
    return 0;
}

static int contains_signed_pre_key(uint32_t signed_pre_key_id, void *user_data)
{
    char key[16];
    snprintf(key, sizeof(key), "spk_%u", (unsigned)signed_pre_key_id);
    size_t len = 0;
    return nvs_get_blob(crypto_nvs_handle(), key, NULL, &len) == ESP_OK ? 1 : 0;
}

static int remove_signed_pre_key(uint32_t signed_pre_key_id, void *user_data)
{
    char key[16];
    snprintf(key, sizeof(key), "spk_%u", (unsigned)signed_pre_key_id);
    esp_err_t err = nvs_erase_key(crypto_nvs_handle(), key);
    nvs_commit(crypto_nvs_handle());
    return err == ESP_OK ? 0 : SG_ERR_UNKNOWN;
}

// --- identity_key_store ---

static int get_identity_key_pair(signal_buffer **public_data, signal_buffer **private_data, void *user_data)
{
    size_t pub_len = 0, priv_len = 0;
    if (nvs_get_blob(crypto_nvs_handle(), "id_pub", NULL, &pub_len) != ESP_OK ||
        nvs_get_blob(crypto_nvs_handle(), "id_priv", NULL, &priv_len) != ESP_OK) {
        return SG_ERR_UNKNOWN;
    }

    uint8_t *pub_buf = malloc(pub_len);
    uint8_t *priv_buf = malloc(priv_len);
    if (!pub_buf || !priv_buf) {
        free(pub_buf);
        free(priv_buf);
        return SG_ERR_NOMEM;
    }
    nvs_get_blob(crypto_nvs_handle(), "id_pub", pub_buf, &pub_len);
    nvs_get_blob(crypto_nvs_handle(), "id_priv", priv_buf, &priv_len);

    *public_data = signal_buffer_create(pub_buf, pub_len);
    *private_data = signal_buffer_create(priv_buf, priv_len);
    free(pub_buf);
    free(priv_buf);
    return (*public_data && *private_data) ? 0 : SG_ERR_NOMEM;
}

static int get_local_registration_id(void *user_data, uint32_t *registration_id)
{
    return nvs_get_u32(crypto_nvs_handle(), "reg_id", registration_id) == ESP_OK ? 0 : SG_ERR_UNKNOWN;
}

static int save_identity(const signal_protocol_address *address, uint8_t *key_data, size_t key_len, void *user_data)
{
    char key[16];
    crypto_node_key((const uint8_t *)address->name, "ik_", key);
    if (!key_data) {
        nvs_erase_key(crypto_nvs_handle(), key);
    } else {
        nvs_set_blob(crypto_nvs_handle(), key, key_data, key_len);
    }
    nvs_commit(crypto_nvs_handle());
    return 0;
}

static int is_trusted_identity(const signal_protocol_address *address, uint8_t *key_data, size_t key_len, void *user_data)
{
    char key[16];
    crypto_node_key((const uint8_t *)address->name, "ik_", key);

    size_t stored_len = 0;
    if (nvs_get_blob(crypto_nvs_handle(), key, NULL, &stored_len) != ESP_OK) {
        return 1; // trust-on-first-use - standardowe zachowanie Signal, rozdz. 6.2 planu
    }

    uint8_t *stored = malloc(stored_len);
    if (!stored) {
        return 0;
    }
    nvs_get_blob(crypto_nvs_handle(), key, stored, &stored_len);
    int trusted = (stored_len == key_len) && (memcmp(stored, key_data, key_len) == 0);
    free(stored);
    return trusted;
}

esp_err_t crypto_store_register(signal_protocol_store_context *store_ctx)
{
    signal_protocol_session_store session_store = {
        .load_session_func = load_session_func,
        .get_sub_device_sessions_func = get_sub_device_sessions_func,
        .store_session_func = store_session_func,
        .contains_session_func = contains_session_func,
        .delete_session_func = delete_session_func,
        .delete_all_sessions_func = delete_all_sessions_func,
        .destroy_func = NULL,
        .user_data = NULL,
    };

    signal_protocol_pre_key_store pre_key_store = {
        .load_pre_key = load_pre_key,
        .store_pre_key = store_pre_key,
        .contains_pre_key = contains_pre_key,
        .remove_pre_key = remove_pre_key,
        .destroy_func = NULL,
        .user_data = NULL,
    };

    signal_protocol_signed_pre_key_store signed_pre_key_store = {
        .load_signed_pre_key = load_signed_pre_key,
        .store_signed_pre_key = store_signed_pre_key,
        .contains_signed_pre_key = contains_signed_pre_key,
        .remove_signed_pre_key = remove_signed_pre_key,
        .destroy_func = NULL,
        .user_data = NULL,
    };

    signal_protocol_identity_key_store identity_key_store = {
        .get_identity_key_pair = get_identity_key_pair,
        .get_local_registration_id = get_local_registration_id,
        .save_identity = save_identity,
        .is_trusted_identity = is_trusted_identity,
        .destroy_func = NULL,
        .user_data = NULL,
    };

    if (signal_protocol_store_context_set_session_store(store_ctx, &session_store) != 0 ||
        signal_protocol_store_context_set_pre_key_store(store_ctx, &pre_key_store) != 0 ||
        signal_protocol_store_context_set_signed_pre_key_store(store_ctx, &signed_pre_key_store) != 0 ||
        signal_protocol_store_context_set_identity_key_store(store_ctx, &identity_key_store) != 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
