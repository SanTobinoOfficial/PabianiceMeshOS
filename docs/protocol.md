# Protokół sieci (format ramki, routing)

Ten dokument opisuje to co faktycznie jest zaimplementowane w
`firmware/components/mesh/` - nie plan na przyszłość, tylko stan bieżący kodu.
Pełne uzasadnienie decyzji projektowych (kalkulacja zasięgu, model zagrożeń,
widełki TTL) jest w `docs/Pabianice_Comms_Plan_Pelny.docx`, rozdz. 5.

## Format ramki

Nagłówek to `pkt_hdr_t` (`firmware/components/mesh/include/pkt.h`), spakowany
bez wyrównania (`__attribute__((packed))`):

| Pole | Rozmiar | Opis |
|---|---|---|
| `type` | 1 B | `PKT_TYPE_DATA` (0x01) / `PKT_TYPE_BEACON` (0x02) / `PKT_TYPE_KEY_BUNDLE` (0x03) |
| `msg_id` | 16 B | losowy identyfikator wiadomości, do deduplikacji |
| `src_id` | 8 B | ID nadawcy (pochodna MAC efuse, patrz `derive_local_id()` w `mesh.c`) |
| `dst_id` | 8 B | ID odbiorcy, zera dla BEACON (rozgłoszeniowy) |
| `ttl` | 1 B | licznik skoków, dekrementowany przy każdym przekazaniu dalej |
| `timestamp` | 4 B | sekundy od startu urządzenia (`esp_timer_get_time() / 1e6`) |
| `payload_len` | 1 B | długość payloadu w bajtach, max `PKT_MAX_PAYLOAD` (200 B) |

Po nagłówku leci payload (do 200 B), a dla DATA docelowo miejsce na sygnaturę
warstwy transmisyjnej (`PKT_SIG_LEN` = 16 B) - klucz sieci wspólny dla całego
mesh, nie klucz użytkownika. **Jeszcze niezaimplementowane, na razie
zerowane** - patrz komentarz w `pkt.h`. Payload DATA to prawdziwy ciphertext
Signal Protocol (X3DH + Double Ratchet, `firmware/components/crypto`);
integralność treści pilnuje już MAC ramki Double Ratchet, więc brak sygnatury
transmisyjnej nie znaczy braku integralności treści - tylko brak
uwierzytelnienia na poziomie "czy ten pakiet w ogóle pochodzi z naszej sieci".

## Typy pakietów

- **DATA (0x01)** - zaszyfrowana wiadomość, flooduje się przez sieć z TTL,
  podlega deduplikacji.
- **BEACON (0x02)** - "jestem tu", TTL zawsze 1 (tylko bezpośredni sąsiedzi,
  rozgłaszanie obecności dalej niż jeden skok nie ma sensu). Aktualizuje
  tabelę obecności (`presence.c`) po stronie odbiorcy, nic więcej.
- **KEY_BUNDLE (0x03)** - wymiana kluczy X3DH między sąsiadami. Subtyp w
  pierwszym bajcie payloadu: `KEY_BUNDLE_REQUEST` (0x00) / `KEY_BUNDLE_RESPONSE`
  (0x01). TTL zawsze 1 - to ograniczenie do jednego skoku, węzły muszą się
  słyszeć bezpośrednio. Zniknie dopiero z katalogiem kluczy na serwerze
  (Faza 3, rozdz. 10.1 planu).

## Routing: flooding + TTL + dedup

Brak trasowania w sensie klasycznym (nie ma tablic routingu) - to epidemic
routing:

1. Węzeł odbiera ramkę, sprawdza `dedup_check_and_mark(msg_id)`
   (`dedup.c`, cache 64 ostatnich ID - starcza z zapasem przy TTL=8 i ruchu
   realistycznym dla kilkunastu-kilkudziesięciu węzłów w zasięgu).
2. Jeśli już widział ten `msg_id` - drop, koniec.
3. Jeśli `dst_id` pasuje do lokalnego ID i typ to DATA - deszyfruje i
   przekazuje do warstwy aplikacji (`mesh_rx_cb_t`).
4. Niezależnie czy pakiet był "dla nas" - jeśli `ttl > 1`, dekrementuje TTL i
   rozgłasza dalej. To epidemic routing: ktoś inny w zasięgu też może tego
   potrzebować, a węzeł pośredniczący i tak nie widzi treści (payload zawsze
   surowy ciphertext, zero-trust, rozdz. 4.3 planu).

TTL domyślny to `PKT_TTL_DEFAULT` = 8 - środek widełek 6-10 z planu (rozdz.
5.2), dobrany pod kalkulację ~300 węzłów / ~32 km² miasta. Do przestrojenia
po pierwszym realnym teście zasięgu (patrz `tutoriale/03-pierwszy-test-sieci.md`).

## Rate-limiting

Per-nadawca licznik w oknie 60s (`RATE_LIMIT_WINDOW_US`), maks. 20 pakietów
na okno (`RATE_LIMIT_MAX_PER_WIN`), tabela na 32 nadawców jednocześnie
(`RATE_TABLE_SIZE`). Bez tego jeden zepsuty albo złośliwy węzeł zalałby całą
sieć floodingiem i zjadł cały dostępny czas antenowy sąsiadom (rozdz. 7.2
planu). Przy przepełnieniu tabeli nowy wpis nadpisuje pierwszy wolny slot -
brak mądrzejszej eviction, świadomy skrót na tym etapie.

## Obecność (beacon)

Każdy węzeł wysyła BEACON co losowy interwał z przedziału 60-120s
(`BEACON_INTERVAL_MIN_US`/`MAX_US`, `presence.c`) - losowość celowa, żeby
węzły w zasięgu nie synchronizowały się i nie biły w eter jednocześnie.
Tabela obecności ma 32 sloty (`PRESENCE_TABLE_SIZE`).

## Znane ograniczenia tego etapu

- Metadane routingu (kto z kim, kiedy, ile) są jawne dla każdego kto
  podsłuchuje transmisję radiową - tylko treść jest szyfrowana E2E (rozdz.
  6.5 planu, znane i udokumentowane, nie błąd).
- Brak fragmentacji: wiadomość, która nie mieści się w `PKT_MAX_PAYLOAD`
  razem z narzutem Double Ratchet (zwłaszcza pierwsza wiadomość sesji, z
  kluczami publicznymi w środku), jest odrzucana, nie ucinana.
- Brak sygnatury warstwy transmisyjnej (pole zarezerwowane, wciąż zerowane).
- ID węzła to nadal skrót MAC (`derive_local_id()`), nie skrót klucza
  publicznego jak docelowo w planie - działa, bo Signal Protocol trzyma
  sesję pod adresem a nie pod samym kluczem, ale traci się weryfikację "kto
  to jest" bez wcześniejszego TOFU. Do ogarnięcia z katalogiem kluczy (Faza 3).
