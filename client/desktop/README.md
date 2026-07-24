# Klient desktopowy

Prawdziwa aplikacja (nie makieta) do łączenia się z siecią Pabianice Comms z komputera,
przez zwykły internet/ethernet - rozmawia z `/server` (store-and-forward + katalog kluczy,
patrz `server/README.md`) po HTTP. Osobna sprawa to telefon bez internetu - do tego
service radiowy (BLE) bezpośrednio do węzła mesh, patrz `client/web-ble/README.md`.

## Dlaczego to nie jest osobna implementacja Signal Protocol

Bo dwie niezależne implementacje tego samego protokołu to gwarancja, że kiedyś się
rozjadą (literówka w serializacji, inna kolejność bajtów, inny brzegowy przypadek) i
klient przestanie dogadywać się z węzłami mesh w sposób, który wychodzi na jaw dopiero
w terenie. Zamiast tego ten klient **kompiluje i linkuje dokładnie te same pliki C**
co firmware:

- `firmware/components/crypto/{provider.c,store.c,pcrypto.c}` - ani jedna linijka
  zmieniona,
- cały `firmware/third_party/libsignal-protocol-c` (git submodule, upstream v2.3.3) -
  ta sama biblioteka X3DH + Double Ratchet.

Jedyny nowy kod to `csrc/host_shim/` - zamiennik wąskiego wycinka API ESP-IDF/FreeRTOS,
którego te pliki używają, żeby dało się je skompilować i uruchomić na zwykłym Linuksie:

| ESP-IDF/FreeRTOS                              | Host shim                                   |
|------------------------------------------------|----------------------------------------------|
| `nvs_get_blob/set_blob/erase_key/commit/*_u32`  | plik na klucz w katalogu tożsamości (`shim.c`) |
| `xSemaphoreCreateRecursiveMutex` + take/give   | `pthread_mutex_t` rekurencyjny                |
| `esp_fill_random`                              | `/dev/urandom`                                |
| `esp_timer_get_time`                            | `clock_gettime(CLOCK_MONOTONIC)`              |
| `ESP_LOGI/ESP_LOGW`                              | `fprintf(stderr, ...)`                        |
| `mbedtls_sha512_update/finish` (ESP-IDF: `int`) | makro na `_ret` (system mbedtls ma je jako stare `void`, patrz `csrc/host_shim/include/mbedtls/sha512.h`) |

`build.rs` kompiluje to wszystko przez `cc` (crate) i linkuje z systemowym `libmbedcrypto`
(pakiet `libmbedtls-dev` na Debianie/Ubuntu - `provider.c` i tak używał tylko mbedtls,
ESP-IDF dostarcza je za darmo, na hoście trzeba doinstalować).

## Format bundla kluczy a serwer

`pcrypto_local_bundle()`/`pcrypto_process_bundle()` w firmware używają własnego,
zwartego formatu binarnego do wymiany bezpośrednio między węzłami przez mesh (limit
199B na pakiet). Serwer (`server/src/routes/keys.rs`) trzyma te same pola, ale jako
osobne hex-owane stringi w JSON. `src/pcrypto.rs` koduje/dekoduje między tymi dwoma
postaciami (patrz `encode_bundle`/`decode_bundle`) - to jedyna warstwa "swojego" kodu w
całej ścieżce kryptograficznej, i to czysto formatowa, nie kryptograficzna.

## Użycie

```bash
cd client/desktop
cargo build --release

# pierwsze uruchomienie generuje tozsamosc (klucz identity + 20 one-time prekeys)
# i wlasny node_id, wszystko w --data-dir (domyslnie ./.pcrypto)
./target/release/pabianice --data-dir ./moje-dane init

# publikuje komplet kluczy publicznych na serwerze pod wlasnym node_id
./target/release/pabianice --data-dir ./moje-dane --server http://twoj-serwer:8080 publish

# wysyla wiadomosc - jesli nie ma jeszcze sesji z peerem, sam pobiera jego bundle
# z serwera i zaklada sesje X3DH
./target/release/pabianice --data-dir ./moje-dane --server http://twoj-serwer:8080 \
    send --to 3eb8758404f1c537 "czesc"

# odbiera i odszyfrowuje czekajace wiadomosci, --ack usuwa je z serwera po odebraniu
./target/release/pabianice --data-dir ./moje-dane --server http://twoj-serwer:8080 poll --ack

# jak wyzej, ale w petli co --interval sekund az do Ctrl-C - zwykle powtarzane
# odpytywanie HTTP, nie push (serwer nie wystawia na zewnatrz swojego wewnetrznego
# kanalu powiadomien Redis) - wygodne do zostawienia w tle w terminalu
./target/release/pabianice --data-dir ./moje-dane --server http://twoj-serwer:8080 \
    listen --interval 5 --ack
```

`--data-dir` to cała tożsamość (klucz prywatny, sesje) - nie kopiuj go między maszynami
(zepsujesz stan Double Ratchet, tak samo jak w prawdziwym Signalu) i nie trzymaj w repo
ani nigdzie publicznie.

## Stan na teraz

Działa naprawdę - przetestowane end-to-end na dwóch osobnych tożsamościach względem
realnie odpalonego `/server` (Postgres + Redis): publikacja bundli, X3DH, wymiana
wiadomości w obie strony, kilka wiadomości w tej samej sesji (Double Ratchet idzie do
przodu, nie tylko pierwsza PreKeySignalMessage), potwierdzanie odbioru (`--ack`),
`listen` odbierający wiadomości wysłane już po jego starcie (przetestowane w tle
z realnym opóźnieniem między wysyłką a odbiorem).

Jednoprocesowy test na 21 kolejnych peerach potwierdził, że po wyczerpaniu startowej
puli 20 one-time prekeys `pcrypto.c` (wspólny z firmware) sam dogenerowuje kolejną
transzę i 21. peer i tak dostaje poprawny bundle - patrz commit dodający
`generate_pre_key_batch`/`next_pre_key_id`.

Osobno przetestowano scenariusz, w którym `/server` akurat nie ma już żadnego
opublikowanego one-time prekey dla danego węzła (bo rozdał jedyny innemu peerowi,
zanim ten węzeł zdążył opublikować nowy) - `pcrypto_process_bundle` i `fetch_bundle`
poprawnie traktują to jako dopuszczalny przypadek X3DH bez OPK (nie błąd), klient
wypisuje ostrzeżenie o słabszym forward secrecy pierwszej wiadomości i mimo to
kończy wymianę sukcesem.

Czego tu jeszcze brakuje:
- `listen` to zwykłe odpytywanie HTTP w pętli (patrz `--interval`), nie push - serwer
  ma wewnętrzny kanał powiadomień (`PUBLISH`/Redis pub-sub pod `notify:<node_id>`),
  ale to infrastruktura pomocnicza dla ewentualnej bramki, nie publiczne API, więc
  klient zewnętrzny i tak musi odpytywać.
- Zero UI - to CLI. Ktoś kto chce okienka, musi je dopisać nad `src/pcrypto.rs`/`src/api.rs`.
