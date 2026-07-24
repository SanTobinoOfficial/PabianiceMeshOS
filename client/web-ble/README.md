# web-ble

Referencyjny klient przeglądarkowy (Web Bluetooth API) - to jest odpowiedź na "z
telefonu bez internetu, przez falę radiową". Zero instalacji, zero konta, zero
serwera: strona łączy się bezpośrednio z jednym, najbliższym węzłem Pabianice Comms po
Bluetooth Low Energy i wysyła/odbiera przez niego wiadomości. Ten węzeł robi za
telefon całą kryptografię (Signal Protocol, X3DH + Double Ratchet) i routing przez
sieć LoRa - telefon nigdy nie ma własnej tożsamości kryptograficznej w tym modelu,
tylko "steruje" węzłem, przy którym akurat stoi.

Protokół (UUID-y, format ramek `[node_id 8B][plaintext]`) opisany w
`firmware/components/ble/README.md` - to tamten komponent (`components/ble`) jest
drugą stroną tego połączenia.

Węzeł wymaga sparowania (bonding, Just Works - patrz jego README, sekcja "Model
zaufania") - przy pierwszym połączeniu system operacyjny telefonu/komputera pokaże
własny systemowy ekran parowania Bluetooth (nie tej strony), zanim wysyłka/odbiór
zadziała. To jednorazowe, kolejne połączenia z tym samym węzłem nie proszą ponownie.

## Uruchomienie

Zwykły statyczny HTML/CSS/JS, bez builda i bez zależności - jak reszta `website/` i
`pabianice-os/admin-panel/`:

```bash
cd client/web-ble
python3 -m http.server 8000
# otworz http://localhost:8000/ w Chrome/Edge/Opera
```

Web Bluetooth wymaga **kontekstu bezpiecznego** (HTTPS albo `localhost`) - zwykły
`http://jakis-adres-ip/` z innej maszyny w sieci nie zadziała, przeglądarka po prostu
nie pokaże przycisku jako aktywnego (patrz `unsupported`/`disabled` w `app.js`).

## Wsparcie przeglądarek - bądź świadomy tego ograniczenia

Web Bluetooth to standard wspierany przez Chrome/Edge/Opera (desktop i Android).
**Safari (macOS i iOS) go nie wspiera i nie zapowiedział wsparcia** - to ograniczenie
WebKita, nie coś do naprawienia po stronie tej strony. Na iPhonie jedyna droga to albo
poczekać na Apple, albo (docelowo) własna aplikacja natywna z CoreBluetooth - poza
zakresem tego, co da się rozsądnie napisać i zweryfikować w tym środowisku (brak
Xcode/symulatora/fizycznego iPhone'a).

## Stan na teraz - szczerze o tym, co nie zostało przetestowane

Strona renderuje się poprawnie i JS parsuje się bez błędów (sprawdzone Playwrightem -
połączenie, formularz, log wiadomości), ale **cała ścieżka Web Bluetooth → prawdziwy
węzeł ESP32 nie została przetestowana end-to-end** - to środowisko, w którym to
powstało, nie ma fizycznego adaptera Bluetooth ani zaprogramowanego węzła do
połączenia się z nim. Logika (parsowanie hex ID, budowanie ramki `[dst_id][plaintext]`,
dekodowanie przychodzących notyfikacji, obsługa rozłączenia) jest napisana zgodnie ze
specyfikacją Web Bluetooth i przetestowana na tyle, na ile dało się bez sprzętu -
faktyczny test na żywym węźle to zadanie dla kogoś z dostępem do zbudowanej płytki
(patrz `tutoriale/01-montaz-i-okablowanie.md` i `02-budowanie-firmware.md`).

## Czego tu jeszcze brakuje

- Zapamiętywania ostatniego połączonego węzła (dziś trzeba wybierać z listy skanowania
  za każdym razem - Web Bluetooth ma do tego `navigator.bluetooth.getDevices()` w
  niektórych przeglądarkach, ale to eksperymentalne API, celowo pominięte).
- Wskaźnika siły sygnału/baterii węzła.
- Historii wiadomości między sesjami (dziś log czyści się przy odświeżeniu strony -
  nic nie zapisuje się do `localStorage`, w duchu "zero danych zostających bez pytania").
