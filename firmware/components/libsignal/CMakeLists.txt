# Wrapper na libsignal-protocol-c (git submodule w firmware/third_party) - biblioteka
# sama w sobie nie ma pojęcia o ESP-IDF, więc trzeba ją tu opakować jako zwykly komponent.
# Cala krypto (ed25519/curve25519, protobuf-c do serializacji) jest zvendorowana w środku
# repo biblioteki, zadnych dodatkowych zaleznosci zewnetrznych (OpenSSL itp.) nie trzeba
# - my dostarczamy tylko crypto_provider (patrz components/crypto/provider.c, oparty o mbedtls).

set(LIBSIGNAL_SRC "${CMAKE_CURRENT_LIST_DIR}/../../third_party/libsignal-protocol-c/src")

idf_component_register(
    SRC_DIRS
        "${LIBSIGNAL_SRC}"
        "${LIBSIGNAL_SRC}/curve25519"
        "${LIBSIGNAL_SRC}/curve25519/ed25519"
        "${LIBSIGNAL_SRC}/curve25519/ed25519/additions"
        "${LIBSIGNAL_SRC}/curve25519/ed25519/additions/generalized"
        "${LIBSIGNAL_SRC}/curve25519/ed25519/nacl_sha512"
        "${LIBSIGNAL_SRC}/curve25519/ed25519/tests"
        "${LIBSIGNAL_SRC}/protobuf-c"
    INCLUDE_DIRS
        "${LIBSIGNAL_SRC}"
        "${LIBSIGNAL_SRC}/curve25519"
        "${LIBSIGNAL_SRC}/curve25519/ed25519"
        "${LIBSIGNAL_SRC}/curve25519/ed25519/nacl_includes"
        "${LIBSIGNAL_SRC}/curve25519/ed25519/additions"
        "${LIBSIGNAL_SRC}/curve25519/ed25519/additions/generalized"
        "${LIBSIGNAL_SRC}/curve25519/ed25519/nacl_sha512"
        "${LIBSIGNAL_SRC}/protobuf-c"
)

# ten kod nie jest pisany pod nasz -Wall, oryginalny build tez to wycisza -
# bez tego log kompilacji tonie w warningach z cudzego kodu
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-sign-compare
    -Wno-sign-conversion
    -Wno-shadow
    -Wno-unused-variable
    -Wno-unused-function
)
