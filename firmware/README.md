# firmware

Kod na ESP32-S3, ESP-IDF (nie Arduino - na razie, może się to zmieni jak ktoś zechce
zrobić wersję uproszczoną dla społeczności, patrz rozdz. 9.2 planu).

## Stan na teraz

Warstwa radiowa (SX1262 po SPI) + prosty routing mesh: flooding z TTL, tabela obecności,
deduplikacja po ID wiadomości, rate-limiting per nadawca. **Bez szyfrowania** - payload
leci jawnym tekstem. Integracja libsignal to następny krok, nie ten.

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
  board/   - piny, SPI bus, nic ponad to
  radio/   - sterownik SX1262 (komendy SPI, konfiguracja LoRa, IRQ na DIO1)
  mesh/    - format pakietu, routing (flooding/TTL/dedup/rate-limit), tabela obecności
main/      - app_main, test aplikacji
```

## Domyślne parametry radiowe

868.1 MHz, SF7, BW125, CR4/5, +14 dBm. Wybrane jako rozsądny start do testów w mieście -
przed podniesieniem mocy nadawania sprawdź aktualne limity ERP dla wybranego podpasma
w tabeli przeznaczeń częstotliwości UKE (rozdz. 3.1 pełnego planu w `docs/`).
