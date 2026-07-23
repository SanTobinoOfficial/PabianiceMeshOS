idf_component_register(
    SRCS "provider.c" "store.c" "pcrypto.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES libsignal nvs_flash mbedtls
    PRIV_REQUIRES esp_timer
)
