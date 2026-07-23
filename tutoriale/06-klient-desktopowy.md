# 6. Klient desktopowy - rozmowa przez internet z komputera

To jest droga dla kogoś, kto ma zwykły dostęp do internetu/ethernetu i chce rozmawiać
przez sieć Pabianice Comms bez stawiania własnego węzła LoRa. Klient (`client/desktop`)
łączy się z `/server` po HTTP i używa dokładnie tego samego kodu Signal Protocol co
firmware - patrz `client/desktop/README.md` po pełne wyjaśnienie jak to działa pod
spodem. Jeśli szukasz sposobu na rozmowę z telefonu **bez internetu**, to inna droga - patrz
sekcja "Telefon bez internetu - most BLE" niżej w tym pliku.

## Wymagania

- Rust (`rustup` - [rustup.rs](https://rustup.rs)),
- kompilator C (`gcc`/`clang` - na Debianie/Ubuntu: `sudo apt install build-essential`),
- `libmbedtls-dev` (`sudo apt install libmbedtls-dev` na Debianie/Ubuntu),
- adres działającego `/server` - albo Twojego własnego (patrz `server/README.md`),
  albo któregoś udostępnionego przez społeczność (Discussions repozytorium).

## Budowanie

```bash
git clone --recurse-submodules https://github.com/SanTobinoOfficial/PabianiceMeshOS
cd PabianiceMeshOS/client/desktop
cargo build --release
```

`--recurse-submodules` jest ważne - klient kompiluje ten sam kod co firmware, w tym
`libsignal-protocol-c` z `firmware/third_party/` (git submodule). Bez tego build się
nie uda (brakujące pliki źródłowe).

## Pierwsze uruchomienie

```bash
# generuje Twoją tożsamość (klucz prywatny + prekeys) i losowy wlasny node_id,
# wszystko zapisane w --data-dir (domyslnie ./.pcrypto obok binarki)
./target/release/pabianice --data-dir ./moje-dane init
```

Zapamiętaj wypisany `node_id` - to Twój adres w sieci, ten sam format co węzły LoRa
(16 znaków hex). `--data-dir` to **cała Twoja tożsamość** - nie kasuj, nie kopiuj
między maszynami, nie wrzucaj do repo/publicznie (to odpowiednik pliku z prywatnym
kluczem Signala).

## Publikacja kluczy

Żeby ktokolwiek mógł do Ciebie napisać, serwer musi znać Twój komplet kluczy
publicznych:

```bash
./target/release/pabianice --data-dir ./moje-dane \
    --server http://adres-twojego-serwera:8080 publish
```

Rób to raz na jakiś czas (np. po każdym uruchomieniu) - serwer trzyma tylko jeden
zestaw naraz, nadpisywanie jest bezpieczne.

## Wysyłanie i odbieranie

```bash
# wysylka - jesli to pierwsza wiadomosc do tego adresata, klient sam pobierze
# jego bundle z serwera i zalozy sesje X3DH, nie trzeba nic robic recznie
./target/release/pabianice --data-dir ./moje-dane --server http://adres:8080 \
    send --to 3eb8758404f1c537 "czesc!"

# odbior - sprawdza czekajace wiadomosci, odszyfrowuje, wypisuje. --ack potwierdza
# odbior (serwer je kasuje) - bez --ack te same wiadomosci beda sie pojawiac przy
# kazdym kolejnym poll
./target/release/pabianice --data-dir ./moje-dane --server http://adres:8080 poll --ack
```

`poll` to jednorazowe sprawdzenie. Żeby zostawić to działające w tle i dostawać
wiadomości na bieżąco, użyj `listen` zamiast pętli `watch`:

```bash
./target/release/pabianice --data-dir ./moje-dane --server http://adres:8080 \
    listen --interval 5 --ack
```

Sprawdza serwer co 5 sekund (`--interval`), wypisuje i (z `--ack`) potwierdza każdą
nową wiadomość, działa aż do Ctrl-C. To wciąż zwykłe odpytywanie HTTP w pętli, nie
prawdziwy push - serwer nie wystawia na zewnątrz swojego wewnętrznego kanału
powiadomień.

## Telefon bez internetu - most BLE

To osobna droga, nie ten klient. Telefon łączy się bezpośrednio z fizycznym węzłem
mesh (ESP32-S3) po Bluetooth Low Energy, bez internetu w ogóle - węzeł robi za niego
całą kryptografię i routing. Otwórz `client/web-ble/index.html` w przeglądarce
(Chrome/Edge/Opera - **nie Safari/iOS**, patrz `client/web-ble/README.md` dlaczego) i
połącz się z widoczną w zasięgu nazwą `pabianice-node`. Wymaga zbudowanego i
wgranego węzła - patrz [tutoriale 1-2](02-budowanie-firmware.md).
