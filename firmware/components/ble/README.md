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

Ten most **nie dodaje żadnego nowego zaufania kryptograficznego** - telefon ufa temu
jednemu wezlowi tylko w zakresie przekazania ruchu (routing), tak jak każdy inny
wezeł w mesh. Treść i tak jest E2E zaszyfrowana Signal Protocol między nadawcą a
odbiorcą - węzeł-brama widzi ciphertext identycznie jak każdy inny węzeł po drodze,
plaintext krąży tylko lokalnie między nim a telefonem po BLE (fizyczna bliskość, nie
eter LoRa).

Samo łącze BLE jest sparowane (bonding, LE Secure Connections, `sm_sc=1`) - węzeł
wymusza parowanie zaraz po połączeniu (`ble_gap_security_initiate` w
`BLE_GAP_EVENT_CONNECT`), a charakterystyka RX dodatkowo ma flagę
`BLE_GATT_CHR_F_WRITE_ENC`, więc zapis jest odrzucany na poziomie ATT, dopóki link nie
jest zaszyfrowany. Klucze bondingu są trwałe (NVS, `CONFIG_BT_NIMBLE_NVS_PERSIST=y`) -
telefon paruje się raz, nie przy każdym połączeniu.

To jest parowanie **Just Works** (`sm_mitm=0`) - węzeł nie ma ekranu ani klawiatury do
wpisania/potwierdzenia PIN-u, więc nie da się tu zrobić uwierzytelnienia odpornego na
aktywny atak man-in-the-middle *w trakcie samego parowania* (ktoś aktywnie
przechwytujący i podszywający się pod obie strony dokładnie w momencie pierwszego
parowania mógłby się wstawić w środek). Chroni to natomiast poprawnie przed biernym
podsłuchem/zapisem przez kogokolwiek innego w zasięgu BLE po sparowaniu - a to był
realny problem wcześniej (dowolny telefon mógł czytać/pisać bez żadnego uwierzytelnienia).
Pełna ochrona przed MITM przy parowaniu wymagałaby wyświetlacza/przycisku na węźle do
potwierdzenia numerycznego (Numeric Comparison) - poza zakresem obecnego sprzętu.

Kilka telefonów może korzystać z jednego węzła naraz (`BLE_MAX_SUBSCRIBERS` = 3,
zgodne z `CONFIG_BT_NIMBLE_MAX_CONNECTIONS` w `sdkconfig.defaults`) - węzeł wznawia
advertising zaraz po każdym udanym połączeniu (nie tylko po rozłączeniu), więc zostaje
widoczny dla kolejnych telefonów, i notyfikuje TX do wszystkich aktualnie
zasubskrybowanych połączeń, nie tylko ostatniego.

## Czego tu jeszcze brakuje

- Ochrony przed MITM w trakcie samego parowania (patrz wyżej - wymaga wyświetlacza na
  węźle, którego obecny sprzęt nie ma).
- Realnego testu na sprzęcie - napisane i zweryfikowane pod kątem zgodności z API
  NimBLE (ESP-IDF v5.2, ten sam target co w `firmware-ci.yml`), ale bez lokalnego
  `idf.py build` (brak zainstalowanego ESP-IDF w środowisku, w którym to powstało) -
  faktyczna weryfikacja kompilacji idzie przez CI po pushu, tak jak reszta firmware
  w tym repo (patrz `firmware/README.md`).
