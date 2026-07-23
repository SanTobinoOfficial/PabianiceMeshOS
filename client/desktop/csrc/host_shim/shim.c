// Implementacja host shimu - pozwala firmware/components/crypto/{provider,store,pcrypto}.c
// skompilowac sie i dzialac na Linuksie bez zmiany choc jednej linii tamtego kodu.
// Zobacz client/desktop/README.md po wyjasnienie dlaczego dzielimy caly ten kod z firmware.

#include "esp_err.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
#include "host_shim.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

// --- katalog danych (per-identity, ustawiany raz przez Rusta przed pcrypto_init) ---

static char s_data_dir[512] = "./pcrypto_data";

void host_shim_set_data_dir(const char *dir)
{
    snprintf(s_data_dir, sizeof(s_data_dir), "%s", dir);
}

// --- semafor rekurencyjny (FreeRTOS -> pthread) ---

struct semphr_shim {
    pthread_mutex_t mutex;
};

SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void)
{
    struct semphr_shim *s = malloc(sizeof(*s));
    if (!s) {
        return NULL;
    }
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&s->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    return s;
}

int xSemaphoreTakeRecursive(SemaphoreHandle_t sem, uint32_t timeout)
{
    (void)timeout; // zawsze portMAX_DELAY w pcrypto.c - blokujemy bezterminowo, jak FreeRTOS
    return pthread_mutex_lock(&sem->mutex) == 0;
}

int xSemaphoreGiveRecursive(SemaphoreHandle_t sem)
{
    return pthread_mutex_unlock(&sem->mutex) == 0;
}

// --- RNG (HW RNG ESP32 -> /dev/urandom) ---

void esp_fill_random(uint8_t *buf, size_t len)
{
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) {
        abort(); // brak entropii = nie generujemy kluczy kryptograficznych, kropka
    }
    if (fread(buf, 1, len, f) != len) {
        fclose(f);
        abort();
    }
    fclose(f);
}

// --- czas (esp_timer_get_time -> CLOCK_MONOTONIC) ---

int64_t esp_timer_get_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

// --- "NVS" jako pliki: katalog na namespace, plik na klucz ---

static void ensure_dir(const char *path)
{
    mkdir(path, 0700); // ignorujemy blad EEXIST - to normalny przypadek
}

static void key_path(nvs_handle_t handle, const char *key, char *out, size_t out_cap)
{
    snprintf(out, out_cap, "%s/ns%d/%s", s_data_dir, handle, key);
}

esp_err_t nvs_open(const char *name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle)
{
    (void)open_mode;
    ensure_dir(s_data_dir);
    char ns_dir[600];
    // jedna aplikacja = jeden namespace w tym kliencie (zawsze "pcrypto"), handle=1 wystarczy
    snprintf(ns_dir, sizeof(ns_dir), "%s/ns1", s_data_dir);
    ensure_dir(ns_dir);
    (void)name;
    *out_handle = 1;
    return ESP_OK;
}

esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length)
{
    char path[700];
    key_path(handle, key, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        return ESP_FAIL;
    }

    if (out_value == NULL) {
        *length = (size_t)size;
        fclose(f);
        return ESP_OK;
    }

    if ((size_t)size > *length) {
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }

    size_t got = fread(out_value, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) {
        return ESP_FAIL;
    }
    *length = got;
    return ESP_OK;
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length)
{
    char path[700];
    key_path(handle, key, path, sizeof(path));

    FILE *f = fopen(path, "wb");
    if (!f) {
        return ESP_FAIL;
    }
    size_t written = fwrite(value, 1, length, f);
    fclose(f);
    return written == length ? ESP_OK : ESP_FAIL;
}

esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key)
{
    char path[700];
    key_path(handle, key, path, sizeof(path));
    return remove(path) == 0 ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    (void)handle;
    return ESP_OK; // kazdy zapis juz idzie od razu na dysk przez fopen/fwrite/fclose
}

esp_err_t nvs_get_u32(nvs_handle_t handle, const char *key, uint32_t *out_value)
{
    size_t len = sizeof(*out_value);
    return nvs_get_blob(handle, key, out_value, &len);
}

esp_err_t nvs_set_u32(nvs_handle_t handle, const char *key, uint32_t value)
{
    return nvs_set_blob(handle, key, &value, sizeof(value));
}
