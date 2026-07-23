// signal_crypto_provider dla libsignal-protocol-c, oparty o mbedtls (jest w ESP-IDF
// za darmo, nie trzeba dociagac OpenSSL). Losowosc idzie bezposrednio z HW RNG ESP32,
// nie przez mbedtls DRBG - jest i szybszy, i nie wymaga wlasnego seedowania.

#include <stdlib.h>
#include <string.h>
#include "internal.h"

#include "esp_random.h"
#include "mbedtls/md.h"
#include "mbedtls/sha512.h"
#include "mbedtls/aes.h"

static int random_func(uint8_t *data, size_t len, void *user_data)
{
    esp_fill_random(data, len);
    return 0;
}

static int hmac_sha256_init_func(void **hmac_context, const uint8_t *key, size_t key_len, void *user_data)
{
    mbedtls_md_context_t *ctx = calloc(1, sizeof(mbedtls_md_context_t));
    if (!ctx) {
        return SG_ERR_NOMEM;
    }
    mbedtls_md_init(ctx);

    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (mbedtls_md_setup(ctx, info, 1) != 0 || mbedtls_md_hmac_starts(ctx, key, key_len) != 0) {
        mbedtls_md_free(ctx);
        free(ctx);
        return SG_ERR_UNKNOWN;
    }
    *hmac_context = ctx;
    return 0;
}

static int hmac_sha256_update_func(void *hmac_context, const uint8_t *data, size_t data_len, void *user_data)
{
    return mbedtls_md_hmac_update((mbedtls_md_context_t *)hmac_context, data, data_len) == 0 ? 0 : SG_ERR_UNKNOWN;
}

static int hmac_sha256_final_func(void *hmac_context, signal_buffer **output, void *user_data)
{
    uint8_t result[32];
    if (mbedtls_md_hmac_finish((mbedtls_md_context_t *)hmac_context, result) != 0) {
        return SG_ERR_UNKNOWN;
    }
    *output = signal_buffer_create(result, sizeof(result));
    return *output ? 0 : SG_ERR_NOMEM;
}

static void hmac_sha256_cleanup_func(void *hmac_context, void *user_data)
{
    mbedtls_md_context_t *ctx = hmac_context;
    if (ctx) {
        mbedtls_md_free(ctx);
        free(ctx);
    }
}

static int sha512_digest_init_func(void **digest_context, void *user_data)
{
    mbedtls_sha512_context *ctx = calloc(1, sizeof(mbedtls_sha512_context));
    if (!ctx) {
        return SG_ERR_NOMEM;
    }
    mbedtls_sha512_init(ctx);
    mbedtls_sha512_starts(ctx, 0); // 0 = SHA-512 pelne, nie wariant SHA-384
    *digest_context = ctx;
    return 0;
}

static int sha512_digest_update_func(void *digest_context, const uint8_t *data, size_t data_len, void *user_data)
{
    return mbedtls_sha512_update((mbedtls_sha512_context *)digest_context, data, data_len) == 0 ? 0 : SG_ERR_UNKNOWN;
}

static int sha512_digest_final_func(void *digest_context, signal_buffer **output, void *user_data)
{
    uint8_t result[64];
    mbedtls_sha512_context *ctx = digest_context;
    if (mbedtls_sha512_finish(ctx, result) != 0) {
        return SG_ERR_UNKNOWN;
    }
    *output = signal_buffer_create(result, sizeof(result));
    // libsignal czasem odpala kolejny digest na tym samym kontekscie bez cleanup
    // miedzy - restartujemy od razu, taniej niz sledzic czy to ostatnie uzycie
    mbedtls_sha512_starts(ctx, 0);
    return *output ? 0 : SG_ERR_NOMEM;
}

static void sha512_digest_cleanup_func(void *digest_context, void *user_data)
{
    mbedtls_sha512_context *ctx = digest_context;
    if (ctx) {
        mbedtls_sha512_free(ctx);
        free(ctx);
    }
}

// PKCS#7 - potrzebne tylko dla trybu CBC, ktorego libsignal w praktyce (na razie)
// nie uzywa do szyfrowania tresci wiadomosci (to CTR), ale provider ma to wspierac
static size_t pkcs7_pad(const uint8_t *in, size_t in_len, uint8_t *out)
{
    size_t padded_len = ((in_len / 16) + 1) * 16;
    uint8_t pad_byte = (uint8_t)(padded_len - in_len);
    memcpy(out, in, in_len);
    memset(out + in_len, pad_byte, pad_byte);
    return padded_len;
}

static int encrypt_func(signal_buffer **output, int cipher,
                         const uint8_t *key, size_t key_len,
                         const uint8_t *iv, size_t iv_len,
                         const uint8_t *plaintext, size_t plaintext_len,
                         void *user_data)
{
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    int ret;
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, iv_len < 16 ? iv_len : 16);

    if (cipher == SG_CIPHER_AES_CTR_NOPADDING) {
        mbedtls_aes_setkey_enc(&aes, key, (unsigned)key_len * 8);
        uint8_t stream_block[16] = { 0 };
        size_t nc_off = 0;
        uint8_t *buf = malloc(plaintext_len);
        if (!buf) { mbedtls_aes_free(&aes); return SG_ERR_NOMEM; }
        ret = mbedtls_aes_crypt_ctr(&aes, plaintext_len, &nc_off, iv_copy, stream_block, plaintext, buf);
        if (ret == 0) {
            *output = signal_buffer_create(buf, plaintext_len);
        }
        free(buf);
    } else if (cipher == SG_CIPHER_AES_CBC_PKCS5) {
        mbedtls_aes_setkey_enc(&aes, key, (unsigned)key_len * 8);
        size_t padded_len = ((plaintext_len / 16) + 1) * 16;
        uint8_t *padded = malloc(padded_len);
        uint8_t *out_buf = malloc(padded_len);
        if (!padded || !out_buf) { free(padded); free(out_buf); mbedtls_aes_free(&aes); return SG_ERR_NOMEM; }
        pkcs7_pad(plaintext, plaintext_len, padded);
        ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded_len, iv_copy, padded, out_buf);
        if (ret == 0) {
            *output = signal_buffer_create(out_buf, padded_len);
        }
        free(padded);
        free(out_buf);
    } else {
        mbedtls_aes_free(&aes);
        return SG_ERR_UNKNOWN;
    }

    mbedtls_aes_free(&aes);
    return (ret == 0 && *output) ? 0 : SG_ERR_UNKNOWN;
}

static int decrypt_func(signal_buffer **output, int cipher,
                        const uint8_t *key, size_t key_len,
                        const uint8_t *iv, size_t iv_len,
                        const uint8_t *ciphertext, size_t ciphertext_len,
                        void *user_data)
{
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    int ret;
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, iv_len < 16 ? iv_len : 16);

    if (cipher == SG_CIPHER_AES_CTR_NOPADDING) {
        mbedtls_aes_setkey_enc(&aes, key, (unsigned)key_len * 8); // CTR: zawsze harmonogram "enc"
        uint8_t stream_block[16] = { 0 };
        size_t nc_off = 0;
        uint8_t *buf = malloc(ciphertext_len);
        if (!buf) { mbedtls_aes_free(&aes); return SG_ERR_NOMEM; }
        ret = mbedtls_aes_crypt_ctr(&aes, ciphertext_len, &nc_off, iv_copy, stream_block, ciphertext, buf);
        if (ret == 0) {
            *output = signal_buffer_create(buf, ciphertext_len);
        }
        free(buf);
    } else if (cipher == SG_CIPHER_AES_CBC_PKCS5) {
        if (ciphertext_len % 16 != 0) { mbedtls_aes_free(&aes); return SG_ERR_UNKNOWN; }
        mbedtls_aes_setkey_dec(&aes, key, (unsigned)key_len * 8);
        uint8_t *buf = malloc(ciphertext_len);
        if (!buf) { mbedtls_aes_free(&aes); return SG_ERR_NOMEM; }
        ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, ciphertext_len, iv_copy, ciphertext, buf);
        if (ret == 0) {
            uint8_t pad = buf[ciphertext_len - 1];
            size_t unpadded_len = (pad > 0 && pad <= 16) ? ciphertext_len - pad : ciphertext_len;
            *output = signal_buffer_create(buf, unpadded_len);
        }
        free(buf);
    } else {
        mbedtls_aes_free(&aes);
        return SG_ERR_UNKNOWN;
    }

    mbedtls_aes_free(&aes);
    return (ret == 0 && *output) ? 0 : SG_ERR_UNKNOWN;
}

void crypto_provider_fill(signal_crypto_provider *provider)
{
    provider->random_func = random_func;
    provider->hmac_sha256_init_func = hmac_sha256_init_func;
    provider->hmac_sha256_update_func = hmac_sha256_update_func;
    provider->hmac_sha256_final_func = hmac_sha256_final_func;
    provider->hmac_sha256_cleanup_func = hmac_sha256_cleanup_func;
    provider->sha512_digest_init_func = sha512_digest_init_func;
    provider->sha512_digest_update_func = sha512_digest_update_func;
    provider->sha512_digest_final_func = sha512_digest_final_func;
    provider->sha512_digest_cleanup_func = sha512_digest_cleanup_func;
    provider->encrypt_func = encrypt_func;
    provider->decrypt_func = decrypt_func;
    provider->user_data = NULL;
}
