#pragma once

// ESP-IDF-owy mbedtls ma mbedtls_sha512_update/finish zwracajace int. System mbedtls na
// Debianie (libmbedtls-dev) trzyma te dwie nazwy jako stare, zwracajace void (deprecated,
// zastapione przez *_ret w 2.7.0) - provider.c (niezmieniony kod z firmware) porownuje ich
// wynik z 0, co się nie kompiluje wprost na hoscie. Przekierowujemy na *_ret przez makra,
// zanim provider.c w ogole zobaczy te nazwy - include_next dociaga prawdziwy naglowek
// systemowy z /usr/include, my tylko dodajemy te dwa aliasy.

#include_next <mbedtls/sha512.h>

#define mbedtls_sha512_update mbedtls_sha512_update_ret
#define mbedtls_sha512_finish mbedtls_sha512_finish_ret
