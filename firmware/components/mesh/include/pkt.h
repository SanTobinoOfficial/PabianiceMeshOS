#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PKT_NODE_ID_LEN  8
#define PKT_MSG_ID_LEN   16
#define PKT_SIG_LEN      16
#define PKT_MAX_PAYLOAD  200

// TTL domyslny - 8 skokow. Plan (rozdz. 5.2) daje widelki 6-10 zaleznie od gestosci
// sieci; 8 to srodek: przy kalkulacji z rozdz. 12 (~300 wezlow / 32km2 miasta) powinno
// starczyc zeby przejsc przez cale Pabianice, a jednoczesnie nie zalewac pasma w gesciej
// zabudowanym centrum. Do przestrojenia po pierwszym realnym tescie zasiegu.
#define PKT_TTL_DEFAULT  8

typedef enum {
    PKT_TYPE_DATA       = 0x01,
    PKT_TYPE_BEACON     = 0x02,
    PKT_TYPE_KEY_BUNDLE = 0x03, // wymiana kluczy X3DH miedzy sasiadami, patrz mesh.c
} pkt_type_t;

// Format ramki wg rozdz. 5.3 planu. Payload DATA to teraz prawdziwy ciphertext Signal
// Protocol (X3DH + Double Ratchet, components/crypto) - integralnosc/autentycznosc
// tresci pilnuje juz MAC w samej ramce Double Ratchet. Signature tutaj to osobna
// sprawa: podpis calego pakietu na poziomie warstwy transmisyjnej (rozdz. 6.3 planu,
// klucz sieci wspolny dla mesh, nie klucz uzytkownika) - jeszcze nie zaimplementowany,
// na razie zerowany.
typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  msg_id[PKT_MSG_ID_LEN];
    uint8_t  src_id[PKT_NODE_ID_LEN];
    uint8_t  dst_id[PKT_NODE_ID_LEN]; // BEACON: nieuzywane, zera
    uint8_t  ttl;
    uint32_t timestamp;
    uint8_t  payload_len;
} pkt_hdr_t;

#define PKT_HDR_LEN ((size_t)sizeof(pkt_hdr_t))
#define PKT_MAX_WIRE_LEN (PKT_HDR_LEN + PKT_MAX_PAYLOAD + PKT_SIG_LEN)

// Pakuje naglowek + payload (+ miejsce na sygnaturę dla DATA) do bufora gotowego do
// wyslania przez radio. Zwraca dlugosc calosci albo 0 jak sie nie zmiescilo w out_cap.
size_t pkt_encode(uint8_t *out, size_t out_cap, const pkt_hdr_t *hdr,
                   const uint8_t *payload, uint8_t payload_len);

// Parsuje surowy bufor odebrany z radia. false = pakiet za krotki albo ucięty payload.
bool pkt_decode(const uint8_t *in, size_t in_len, pkt_hdr_t *hdr_out,
                const uint8_t **payload_out);
