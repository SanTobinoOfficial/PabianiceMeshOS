// Warstwa kryptograficzna - X3DH + Double Ratchet przez libsignal-protocol-c (rozdz. 6
// planu). To jest ta "porcelanowa" strona: mesh.c wola tylko funkcje z pcrypto.h, cala
// hydraulika ze store'ami/providerem siedzi w store.c/provider.c.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "pcrypto.h"
#include "internal.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "key_helper.h"
#include "session_builder.h"
#include "session_cipher.h"
#include "session_pre_key.h"
#include "curve.h"
#include "protocol.h"

static const char *TAG = "pcrypto";

// tyle one-time prekeys generujemy na start i tyle samo dogenerowujemy kazdorazowo
// po wyczerpaniu puli (patrz generate_pre_key_batch/next_pre_key_id nizej) - ID
// nigdy nie sa uzywane ponownie, nowa transza zawsze zaczyna sie od najwyzszego
// kiedykolwiek wygenerowanego ID + 1
#define PRE_KEY_BATCH_SIZE 20
#define SIGNED_PRE_KEY_ID  1

static nvs_handle_t s_nvs;
static signal_context *s_ctx;
static signal_protocol_store_context *s_store;
static SemaphoreHandle_t s_lock;

nvs_handle_t crypto_nvs_handle(void) { return s_nvs; }
signal_context *crypto_signal_ctx(void) { return s_ctx; }
signal_protocol_store_context *crypto_store_ctx(void) { return s_store; }

static void lock_func(void *user_data)
{
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
}

static void unlock_func(void *user_data)
{
    xSemaphoreGiveRecursive(s_lock);
}

// Generuje `count` nowych one-time prekeys od `start_id`, zapisuje w NVS i przesuwa
// "next_pk_id" o `count` do przodu - to ten licznik pilnuje, zeby po wyczerpaniu
// pierwszej transzy nie generowac ID, ktore juz kiedys istnialo (patrz next_pre_key_id)
static esp_err_t generate_pre_key_batch(uint32_t start_id, int count)
{
    signal_protocol_key_helper_pre_key_list_node *head = NULL;
    if (signal_protocol_key_helper_generate_pre_keys(&head, start_id, count, s_ctx) != 0) {
        return ESP_FAIL;
    }
    for (signal_protocol_key_helper_pre_key_list_node *n = head; n != NULL;
         n = signal_protocol_key_helper_key_list_next(n)) {
        session_pre_key *pk = signal_protocol_key_helper_key_list_element(n);
        signal_buffer *rec = NULL;
        session_pre_key_serialize(&rec, pk);
        char key[16];
        snprintf(key, sizeof(key), "pk_%u", (unsigned)session_pre_key_get_id(pk));
        nvs_set_blob(s_nvs, key, signal_buffer_data(rec), signal_buffer_len(rec));
        signal_buffer_free(rec);
    }
    signal_protocol_key_helper_key_list_free(head);

    nvs_set_u32(s_nvs, "next_pk_id", start_id + (uint32_t)count);
    nvs_commit(s_nvs);
    return ESP_OK;
}

// Najwyzsze kiedykolwiek wygenerowane ID + 1. Brak "next_pk_id" w NVS oznacza
// tozsamosc sprzed wprowadzenia dogenerowywania prekeys - taka ma dokladnie
// PRE_KEY_BATCH_SIZE wygenerowanych na starcie (patrz identity_ensure_generated nizej)
static uint32_t next_pre_key_id(void)
{
    uint32_t next = 0;
    if (nvs_get_u32(s_nvs, "next_pk_id", &next) != ESP_OK) {
        next = PRE_KEY_BATCH_SIZE + 1;
    }
    return next;
}

static esp_err_t identity_ensure_generated(void)
{
    size_t len = 0;
    if (nvs_get_blob(s_nvs, "id_pub", NULL, &len) == ESP_OK) {
        return ESP_OK; // juz mamy tozsamosc z poprzedniego rozruchu
    }

    ratchet_identity_key_pair *identity = NULL;
    if (signal_protocol_key_helper_generate_identity_key_pair(&identity, s_ctx) != 0) {
        return ESP_FAIL;
    }

    signal_buffer *pub_buf = NULL, *priv_buf = NULL;
    ec_public_key_serialize(&pub_buf, ratchet_identity_key_pair_get_public(identity));
    ec_private_key_serialize(&priv_buf, ratchet_identity_key_pair_get_private(identity));
    nvs_set_blob(s_nvs, "id_pub", signal_buffer_data(pub_buf), signal_buffer_len(pub_buf));
    nvs_set_blob(s_nvs, "id_priv", signal_buffer_data(priv_buf), signal_buffer_len(priv_buf));
    signal_buffer_free(pub_buf);
    signal_buffer_free(priv_buf);

    uint32_t reg_id = 0;
    signal_protocol_key_helper_generate_registration_id(&reg_id, 0, s_ctx);
    nvs_set_u32(s_nvs, "reg_id", reg_id);

    if (generate_pre_key_batch(1, PRE_KEY_BATCH_SIZE) != ESP_OK) {
        SIGNAL_UNREF(identity);
        return ESP_FAIL;
    }

    // timestamp od bootu, nie zegar scienny - nie mamy jeszcze NTP/RTC. Nie
    // sprawdzamy nigdzie wieku signed prekey, wiec na razie to tylko wypelniacz pola
    uint64_t boot_ms = (uint64_t)(esp_timer_get_time() / 1000);
    session_signed_pre_key *spk = NULL;
    signal_protocol_key_helper_generate_signed_pre_key(&spk, identity, SIGNED_PRE_KEY_ID, boot_ms, s_ctx);
    signal_buffer *spk_rec = NULL;
    session_signed_pre_key_serialize(&spk_rec, spk);
    char spk_key[16];
    snprintf(spk_key, sizeof(spk_key), "spk_%u", (unsigned)SIGNED_PRE_KEY_ID);
    nvs_set_blob(s_nvs, spk_key, signal_buffer_data(spk_rec), signal_buffer_len(spk_rec));
    signal_buffer_free(spk_rec);

    nvs_commit(s_nvs);

    SIGNAL_UNREF(spk);
    SIGNAL_UNREF(identity);

    ESP_LOGI(TAG, "wygenerowano nowa tozsamosc (registration_id=%u, %d prekeys)",
             (unsigned)reg_id, PRE_KEY_BATCH_SIZE);
    return ESP_OK;
}

esp_err_t pcrypto_init(void)
{
    s_lock = xSemaphoreCreateRecursiveMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = nvs_open("pcrypto", NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) {
        return err;
    }

    if (signal_context_create(&s_ctx, NULL) != 0) {
        return ESP_FAIL;
    }
    signal_context_set_locking_functions(s_ctx, lock_func, unlock_func);

    signal_crypto_provider provider = { 0 };
    crypto_provider_fill(&provider);
    signal_context_set_crypto_provider(s_ctx, &provider);

    if (signal_protocol_store_context_create(&s_store, s_ctx) != 0) {
        return ESP_FAIL;
    }
    if (crypto_store_register(s_store) != ESP_OK) {
        return ESP_FAIL;
    }

    return identity_ensure_generated();
}

static esp_err_t find_unused_pre_key(uint32_t *id_out, uint8_t *pub_out, size_t pub_cap, size_t *pub_len_out)
{
    uint32_t highest = next_pre_key_id() - 1;
    for (uint32_t id = 1; id <= highest; id++) {
        char key[16];
        snprintf(key, sizeof(key), "pk_%u", (unsigned)id);
        size_t len = 0;
        if (nvs_get_blob(s_nvs, key, NULL, &len) != ESP_OK) {
            continue; // juz zuzyty (session_builder go usuwa po stronie odbiorcy bundla)
        }
        uint8_t *buf = malloc(len);
        if (!buf) {
            continue;
        }
        nvs_get_blob(s_nvs, key, buf, &len);

        session_pre_key *pk = NULL;
        int rc = session_pre_key_deserialize(&pk, buf, len, s_ctx);
        free(buf);
        if (rc != 0) {
            continue;
        }

        signal_buffer *pub_buf = NULL;
        ec_public_key_serialize(&pub_buf, ec_key_pair_get_public(session_pre_key_get_key_pair(pk)));
        size_t plen = signal_buffer_len(pub_buf);
        if (plen <= pub_cap) {
            memcpy(pub_out, signal_buffer_data(pub_buf), plen);
            *pub_len_out = plen;
            *id_out = id;
            signal_buffer_free(pub_buf);
            SIGNAL_UNREF(pk);
            return ESP_OK;
        }
        signal_buffer_free(pub_buf);
        SIGNAL_UNREF(pk);
    }
    return ESP_ERR_NOT_FOUND;
}

static bool bundle_put_u32(pcrypto_bundle_t *out, size_t *off, uint32_t v)
{
    if (*off + 4 > sizeof(out->data)) {
        return false;
    }
    memcpy(out->data + *off, &v, 4);
    *off += 4;
    return true;
}

static bool bundle_put_blob(pcrypto_bundle_t *out, size_t *off, const uint8_t *data, size_t len)
{
    if (len > 255 || *off + 1 + len > sizeof(out->data)) {
        return false;
    }
    out->data[(*off)++] = (uint8_t)len;
    memcpy(out->data + *off, data, len);
    *off += len;
    return true;
}

esp_err_t pcrypto_local_bundle(pcrypto_bundle_t *out)
{
    uint32_t reg_id = 0;
    if (nvs_get_u32(s_nvs, "reg_id", &reg_id) != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t pre_key_id = 0;
    uint8_t pre_key_pub[64];
    size_t pre_key_pub_len = 0;
    if (find_unused_pre_key(&pre_key_id, pre_key_pub, sizeof(pre_key_pub), &pre_key_pub_len) != ESP_OK) {
        // pula wyczerpana - dogenerowujemy nowa transze (tyle co na starcie) i probujemy
        // jeszcze raz, zamiast od razu zglaszac ze nie ma juz czym zaczynac nowych sesji
        if (generate_pre_key_batch(next_pre_key_id(), PRE_KEY_BATCH_SIZE) != ESP_OK) {
            return ESP_ERR_NOT_FOUND;
        }
        ESP_LOGI(TAG, "pula one-time prekeys byla pusta, dogenerowano %d nowych", PRE_KEY_BATCH_SIZE);
        if (find_unused_pre_key(&pre_key_id, pre_key_pub, sizeof(pre_key_pub), &pre_key_pub_len) != ESP_OK) {
            return ESP_ERR_NOT_FOUND;
        }
    }

    char spk_key[16];
    snprintf(spk_key, sizeof(spk_key), "spk_%u", (unsigned)SIGNED_PRE_KEY_ID);
    size_t spk_len = 0;
    if (nvs_get_blob(s_nvs, spk_key, NULL, &spk_len) != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t *spk_buf = malloc(spk_len);
    if (!spk_buf) {
        return ESP_ERR_NO_MEM;
    }
    nvs_get_blob(s_nvs, spk_key, spk_buf, &spk_len);
    session_signed_pre_key *spk = NULL;
    int rc = session_signed_pre_key_deserialize(&spk, spk_buf, spk_len, s_ctx);
    free(spk_buf);
    if (rc != 0) {
        return ESP_FAIL;
    }

    signal_buffer *spk_pub_buf = NULL;
    ec_public_key_serialize(&spk_pub_buf, ec_key_pair_get_public(session_signed_pre_key_get_key_pair(spk)));
    const uint8_t *sig = session_signed_pre_key_get_signature(spk);
    size_t sig_len = session_signed_pre_key_get_signature_len(spk);
    uint32_t signed_pre_key_id = session_signed_pre_key_get_id(spk);

    size_t idpub_len = 0;
    if (nvs_get_blob(s_nvs, "id_pub", NULL, &idpub_len) != ESP_OK) {
        signal_buffer_free(spk_pub_buf);
        SIGNAL_UNREF(spk);
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t *idpub_buf = malloc(idpub_len);
    if (!idpub_buf) {
        signal_buffer_free(spk_pub_buf);
        SIGNAL_UNREF(spk);
        return ESP_ERR_NO_MEM;
    }
    nvs_get_blob(s_nvs, "id_pub", idpub_buf, &idpub_len);

    // wlasny prosty format do przeslania przez mesh - libsignal nie narzuca formatu
    // wymiany bundla (to zwykle robilby serwer, rozdz. 10.1), a kroku 3 jeszcze nie ma
    size_t off = 0;
    bool ok = bundle_put_u32(out, &off, reg_id)
        && bundle_put_u32(out, &off, pre_key_id)
        && bundle_put_blob(out, &off, pre_key_pub, pre_key_pub_len)
        && bundle_put_u32(out, &off, signed_pre_key_id)
        && bundle_put_blob(out, &off, signal_buffer_data(spk_pub_buf), signal_buffer_len(spk_pub_buf))
        && bundle_put_blob(out, &off, sig, sig_len)
        && bundle_put_blob(out, &off, idpub_buf, idpub_len);

    free(idpub_buf);
    signal_buffer_free(spk_pub_buf);
    SIGNAL_UNREF(spk);

    if (!ok) {
        return ESP_ERR_INVALID_SIZE;
    }
    out->len = off;
    return ESP_OK;
}

esp_err_t pcrypto_process_bundle(const uint8_t peer_id[PCRYPTO_PEER_ID_LEN], const uint8_t *data, size_t len)
{
    size_t off = 0;

#define NEED(n) do { if (off + (n) > len) return ESP_ERR_INVALID_ARG; } while (0)

    NEED(4);
    uint32_t reg_id;
    memcpy(&reg_id, data + off, 4);
    off += 4;

    NEED(4);
    uint32_t pre_key_id;
    memcpy(&pre_key_id, data + off, 4);
    off += 4;

    NEED(1);
    uint8_t pre_key_pub_len = data[off++];
    NEED(pre_key_pub_len);
    const uint8_t *pre_key_pub_data = data + off;
    off += pre_key_pub_len;

    NEED(4);
    uint32_t signed_pre_key_id;
    memcpy(&signed_pre_key_id, data + off, 4);
    off += 4;

    NEED(1);
    uint8_t spk_pub_len = data[off++];
    NEED(spk_pub_len);
    const uint8_t *spk_pub_data = data + off;
    off += spk_pub_len;

    NEED(1);
    uint8_t sig_len = data[off++];
    NEED(sig_len);
    const uint8_t *sig_data = data + off;
    off += sig_len;

    NEED(1);
    uint8_t idpub_len = data[off++];
    NEED(idpub_len);
    const uint8_t *idpub_data = data + off;
    off += idpub_len;

#undef NEED

    ec_public_key *pre_key_pub = NULL, *spk_pub = NULL, *identity_pub = NULL;
    if (curve_decode_point(&pre_key_pub, pre_key_pub_data, pre_key_pub_len, s_ctx) != 0) {
        return ESP_FAIL;
    }
    if (curve_decode_point(&spk_pub, spk_pub_data, spk_pub_len, s_ctx) != 0) {
        SIGNAL_UNREF(pre_key_pub);
        return ESP_FAIL;
    }
    if (curve_decode_point(&identity_pub, idpub_data, idpub_len, s_ctx) != 0) {
        SIGNAL_UNREF(pre_key_pub);
        SIGNAL_UNREF(spk_pub);
        return ESP_FAIL;
    }

    session_pre_key_bundle *bundle = NULL;
    int rc = session_pre_key_bundle_create(&bundle, reg_id, 1 /* device_id */,
                                            pre_key_id, pre_key_pub,
                                            signed_pre_key_id, spk_pub,
                                            sig_data, sig_len,
                                            identity_pub);

    esp_err_t out_err = ESP_OK;
    if (rc != 0) {
        out_err = ESP_FAIL;
    } else {
        signal_protocol_address addr = {
            .name = (const char *)peer_id,
            .name_len = PCRYPTO_PEER_ID_LEN,
            .device_id = 1,
        };
        session_builder *builder = NULL;
        if (session_builder_create(&builder, s_store, &addr, s_ctx) != 0) {
            out_err = ESP_FAIL;
        } else {
            // rc < 0 tutaj to najczesciej SG_ERR_UNTRUSTED_IDENTITY - ktos podszywa sie
            // pod juz znany identity key peera, albo SG_ERR_INVALID_KEY - zly bundle
            if (session_builder_process_pre_key_bundle(builder, bundle) != 0) {
                out_err = ESP_FAIL;
            }
            session_builder_free(builder);
        }
    }

    // bundle trzyma wlasna referencje (ref-counting) do przekazanych kluczy, wiec nasze
    // lokalne referencje trzeba zwolnic osobno niezaleznie od bundla
    if (bundle) {
        SIGNAL_UNREF(bundle);
    }
    SIGNAL_UNREF(pre_key_pub);
    SIGNAL_UNREF(spk_pub);
    SIGNAL_UNREF(identity_pub);

    return out_err;
}

bool pcrypto_has_session(const uint8_t peer_id[PCRYPTO_PEER_ID_LEN])
{
    signal_protocol_address addr = {
        .name = (const char *)peer_id,
        .name_len = PCRYPTO_PEER_ID_LEN,
        .device_id = 1,
    };
    return signal_protocol_session_contains_session(s_store, &addr) > 0;
}

esp_err_t pcrypto_encrypt(const uint8_t peer_id[PCRYPTO_PEER_ID_LEN],
                          const uint8_t *plaintext, size_t plaintext_len,
                          uint8_t *out, size_t out_cap, size_t *out_len)
{
    signal_protocol_address addr = {
        .name = (const char *)peer_id,
        .name_len = PCRYPTO_PEER_ID_LEN,
        .device_id = 1,
    };

    session_cipher *cipher = NULL;
    if (session_cipher_create(&cipher, s_store, &addr, s_ctx) != 0) {
        return ESP_FAIL;
    }

    ciphertext_message *msg = NULL;
    int rc = session_cipher_encrypt(cipher, plaintext, plaintext_len, &msg);
    if (rc != 0) {
        session_cipher_free(cipher);
        return ESP_FAIL;
    }

    // doklejamy typ wiadomosci (PreKeySignalMessage vs zwykla) przed serializacja -
    // libsignal go nie transportuje sam, w prawdziwym Signalu robi to koperta serwera
    int msg_type = ciphertext_message_get_type(msg);
    signal_buffer *serialized = ciphertext_message_get_serialized(msg);
    size_t len = signal_buffer_len(serialized);

    esp_err_t err;
    if (1 + len > out_cap) {
        err = ESP_ERR_INVALID_SIZE;
    } else {
        out[0] = (uint8_t)msg_type;
        memcpy(out + 1, signal_buffer_data(serialized), len);
        *out_len = 1 + len;
        err = ESP_OK;
    }

    SIGNAL_UNREF(msg);
    session_cipher_free(cipher);
    return err;
}

esp_err_t pcrypto_decrypt(const uint8_t peer_id[PCRYPTO_PEER_ID_LEN],
                          const uint8_t *ciphertext, size_t ciphertext_len,
                          uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (ciphertext_len < 1) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t msg_type = ciphertext[0];
    const uint8_t *body = ciphertext + 1;
    size_t body_len = ciphertext_len - 1;

    signal_protocol_address addr = {
        .name = (const char *)peer_id,
        .name_len = PCRYPTO_PEER_ID_LEN,
        .device_id = 1,
    };
    session_cipher *cipher = NULL;
    if (session_cipher_create(&cipher, s_store, &addr, s_ctx) != 0) {
        return ESP_FAIL;
    }

    signal_buffer *plaintext = NULL;
    int rc;

    if (msg_type == CIPHERTEXT_PREKEY_TYPE) {
        pre_key_signal_message *msg = NULL;
        rc = pre_key_signal_message_deserialize(&msg, body, body_len, s_ctx);
        if (rc == 0) {
            rc = session_cipher_decrypt_pre_key_signal_message(cipher, msg, NULL, &plaintext);
            SIGNAL_UNREF(msg);
        }
    } else if (msg_type == CIPHERTEXT_SIGNAL_TYPE) {
        signal_message *msg = NULL;
        rc = signal_message_deserialize(&msg, body, body_len, s_ctx);
        if (rc == 0) {
            rc = session_cipher_decrypt_signal_message(cipher, msg, NULL, &plaintext);
            SIGNAL_UNREF(msg);
        }
    } else {
        rc = SG_ERR_INVALID_MESSAGE;
    }

    session_cipher_free(cipher);

    if (rc != 0 || !plaintext) {
        ESP_LOGW(TAG, "odszyfrowanie nie wyszlo (rc=%d)", rc);
        return ESP_FAIL;
    }

    size_t len = signal_buffer_len(plaintext);
    esp_err_t err = ESP_OK;
    if (len > out_cap) {
        err = ESP_ERR_INVALID_SIZE;
    } else {
        memcpy(out, signal_buffer_data(plaintext), len);
        *out_len = len;
    }
    signal_buffer_free(plaintext);
    return err;
}
