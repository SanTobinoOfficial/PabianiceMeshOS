# firmware

Kod na ESP32-S3, ESP-IDF (nie Arduino - na razie, może się to zmieni jak ktoś zechce
zrobić wersję uproszczoną dla społeczności, patrz rozdz. 9.2 planu).

## Stan na teraz

Warstwa radiowa (SX1262 po SPI) + routing mesh (flooding z TTL, tabela obecności,
deduplikacja po ID wiadomości, rate-limiting per nadawca) + szyfrowanie end-to-end
(Signal Protocol - X3DH + Double Ratchet, przez `libsignal-protocol-c`). Wiadomości
DATA sa juz szyfrowane; wymiana kluczy miedzy dwoma wezlami na razie idzie bezposrednio
przez mesh (bez katalogu kluczy na serwerze - to dopiero krok 3).

## Pobieranie (submoduly!)

`libsignal-protocol-c` jest podpiety jako git submodule (`third_party/`), samo
`git clone` tego repo go nie sciaga:

```
git submodule update --init --recursive
```

## Budowanie

Wymaga ESP-IDF (testowane na v5.x, target `esp32s3`):

```
. $HOME/esp/esp-idf/export.sh
cd firmware
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Świeży projekt, więc pierwsze `idf.py build` może się wywalić na brakujących
`REQUIRES` w którymś CMakeLists.txt komponentu - jeszcze nie miałem okazji zbudować
tego na czystym środowisku, dopisz czego brakuje.

## Okablowanie (devkit ESP32-S3 + moduł SX1262)

Domyślne piny zdefiniowane w `components/board/include/board.h`:

| Sygnał | Pin ESP32-S3 |
|--------|--------------|
| MOSI   | GPIO11       |
| MISO   | GPIO13       |
| SCLK   | GPIO12       |
| NSS    | GPIO10       |
| RESET  | GPIO9        |
| BUSY   | GPIO8        |
| DIO1   | GPIO7        |

Inny devboard albo inne rozmieszczenie pinów na Twojej płytce - po prostu zmień te
definicje, reszta kodu ich nie zna.

## Struktura

```
components/
  board/     - piny, SPI bus, nic ponad to
  radio/     - sterownik SX1262 (komendy SPI, konfiguracja LoRa, IRQ na DIO1)
  mesh/      - format pakietu, routing (flooding/TTL/dedup/rate-limit), tabela obecności
  crypto/    - X3DH + Double Ratchet: provider (mbedtls), store (NVS), API dla mesh.c
  libsignal/ - CMake-owy wrapper na third_party/libsignal-protocol-c (submodule)
main/        - app_main, test aplikacji
third_party/
  libsignal-protocol-c/ - git submodule, gorna warstwa Signal Protocol
```

## Klucze i tozsamosc

Identity key, prekeys i sesje Double Ratchet siedza w NVS (flash), nie w module
bezpiecznym - ATECC608A jeszcze nie jest zamontowany (patrz BOM w rozdz. 8.1 planu).
To znaczy, ze klucz prywatny da sie wyciagnac z flasha kims z dostepem fizycznym do
plytki. Do przetestowania protokolu to wystarczy, do realnego uzycia - nie, to jest
jawny dlug techniczny do spłacenia zanim to trafi w czyjes rece poza testowym gronem.

## Domyślne parametry radiowe

868.1 MHz, SF7, BW125, CR4/5, +14 dBm. Wybrane jako rozsądny start do testów w mieście -
przed podniesieniem mocy nadawania sprawdź aktualne limity ERP dla wybranego podpasma
w tabeli przeznaczeń częstotliwości UKE (rozdz. 3.1 pełnego planu w `docs/`).
