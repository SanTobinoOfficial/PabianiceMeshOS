# ble

Most GATT (NimBLE) miedzy telefonem a siecia mesh - to jest odpowiedz na "z telefonu
bez internetu, przez fale radiowa". Telefon nie gada z LoRa bezposrednio (nie ma do
tego anteny/modulu), tylko laczy sie po BLE z jednym, najblizszym wezlem Pabianice
Comms - ten wezel routuje, szyfruje/odszyfrowuje (Signal Protocol) i przekazuje dalej
przez LoRa mesh, dokladnie tak samo jakby to robil dla samego siebie. Zero internetu
w tej sciezce.

Referencyjny klient przegladarkowy (Web Bluetooth) w `client/web-ble/`.

## Protokol (custom GATT service)

Jedna usluga, dwie charakterystyki, UUID-y specyficzne dla tego projektu (nie
zarejestrowane w Bluetooth SIG - to nie problem, 128-bitowe custom UUID sa do tego
i tak przeznaczone):

| Co | UUID |
|---|---|
| Service | `2b195cc7-9a84-4b1a-8f6e-112233440001` |
| Charakterystyka RX (telefon → wezel, write) | `2b195cc7-9a84-4b1a-8f6e-112233440002` |
| Charakterystyka TX (wezel → telefon, notify) | `2b195cc7-9a84-4b1a-8f6e-112233440003` |

Nazwa urzadzenia przy skanowaniu: `pabianice-node`.

Format ramek - identyczny w obie strony, 8 bajtow ID węzła + reszta to plaintext
(TEN WĘZEŁ szyfruje/odszyfrowuje po swojej stronie, telefon nigdy nie widzi ani nie
tworzy ciphertextu):

```
RX (write):    [dst_id: 8B][plaintext: reszta]   -> wezel wola mesh_send(dst_id, plaintext, ...)
TX (notify):   [src_id: 8B][plaintext: reszta]   <- wezel dostal to z mesh_set_rx_callback
```

`dst_id`/`src_id` to te same 8-bajtowe ID co wszędzie indziej w projekcie (`PKT_NODE_ID_LEN`,
`server/src/nodeid.rs`, `client/desktop`) - w UI zwykle jako 16 znaków hex.

Serwer GATT wystawia preferowane MTU 247 zaraz po starcie (`ble_att_set_preferred_mtu`),
bo domyślne 23B ATT MTU nie pomieści nawet nagłówka (8B ID) + sensownej wiadomości
(limit payloadu w mesh to `PKT_MAX_PAYLOAD` = 200B). Telefon/przeglądarka powinien
zaakceptować negocjację MTU - Web Bluetooth robi to sam, nie trzeba nic wołać ręcznie.

## Model zaufania

Ten most **nie dodaje żadnego nowego zaufania** - telefon ufa temu jednemu wezlowi
tylko w zakresie przekazania ruchu (routing), tak jak każdy inny wezeł w mesh. Treść
i tak jest E2E zaszyfrowana Signal Protocol między nadawcą a odbiorcą - węzeł-brama
widzi ciphertext identycznie jak każdy inny węzeł po drodze, plaintext krąży tylko
lokalnie między nim a telefonem po BLE (fizyczna bliskość, nie eter LoRa). BLE samo w
sobie nie jest szyfrowane na tym etapie (`ble_gap_adv_params`/`sync_cb` nie ustawiają
pairing/bonding) - do zrobienia zanim to wyjdzie poza testy na stole, patrz "Czego
brakuje" niżej.

## Czego tu jeszcze brakuje

- Parowania/szyfrowania samego łącza BLE (dziś dowolny telefon w zasięgu może się
  połączyć i pisać/czytać na tej usłudze - w praktyce ograniczone tylko fizyczną
  bliskością, ale to nie jest security boundary).
- Wsparcia wielu jednoczesnych połączeń telefonów do jednego węzła (NimBLE to
  udźwignie, ten kod na razie śledzi tylko jedno `conn_handle` dla TX notify).
- Realnego testu na sprzęcie - napisane i zweryfikowane pod kątem zgodności z API
  NimBLE (ESP-IDF v5.2, ten sam target co w `firmware-ci.yml`), ale bez lokalnego
  `idf.py build` (brak zainstalowanego ESP-IDF w środowisku, w którym to powstało) -
  faktyczna weryfikacja kompilacji idzie przez CI po pushu, tak jak reszta firmware
  w tym repo (patrz `firmware/README.md`).
