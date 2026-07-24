# Status: gotowość do przejścia z Fazy 1 do Fazy 2

Ten dokument śledzi konkretny postęp względem rozdz. 11.1/11.2 pełnego planu
(`docs/Pabianice_Comms_Plan_Pelny.docx`) - checklisty, nie prozy, żeby było jasne
gołym okiem co jest zrobione, a co nie, i **kto** ma to zrobić (agent prowadzący
repo może dowieźć tylko część - reszta wymaga działania w świecie fizycznym).

## Faza 1 - Prototyp dla wąskiego grona (rozdz. 11.1)

- [x] Implementacja podstawowej wersji protokołu mesh i szyfrowania Signal Protocol -
  routing (flooding/TTL/dedup/rate-limit), X3DH + Double Ratchet przez
  `libsignal-protocol-c`, patrz `firmware/README.md`.
- [ ] **Zbudowanie 3-5 prototypów urządzeń zgodnie z BOM (rozdz. 8)** - wymaga
  fizycznego sprzętu (ESP32-S3, moduł SX1262, okablowanie) i rąk przy lutownicy.
  Nie da się tego zrobić w tym środowisku (sandboksowany kontener bez dostępu do
  jakiegokolwiek sprzętu radiowego) - **to zadanie dla właściciela projektu**,
  patrz `tutoriale/01-montaz-i-okablowanie.md`.
- [ ] **Test rzeczywistego zasięgu radiowego w warunkach miejskich Pabianic** -
  wymaga fizycznej obecności w mieście z zbudowanymi urządzeniami. Kalkulacje
  teoretyczne są w rozdz. 12 planu, ale to *pomiar*, nie symulacja - **zadanie dla
  właściciela projektu**, patrz `tutoriale/03-pierwszy-test-sieci.md`.
- [ ] Użytkowanie wyłącznie w gronie osobiście znanych osób, bez publicznego
  udostępniania - to praktyka/decyzja właściciela projektu w trakcie testów, nie
  coś co koduje się w repozytorium.

## Faza 2 - Publikacja jako open source (rozdz. 11.2)

- [x] Publikacja kodu (firmware, `/server`, protokół, `/pabianice-os`, klienci) na
  GitHub pod licencją open source - `LICENSE` to GNU GPLv3, repozytorium publiczne.
- [x] Jasne zastrzeżenie, że użytkownik sam odpowiada za zgodność zbudowanego
  urządzenia z lokalnymi przepisami radiowymi (ERP, duty cycle) - `README.md`,
  sekcja główna, oraz `firmware/README.md`.
- [x] Sieć rośnie w trybie w pełni lokalnym (peer-to-peer), bez zależności od
  serwera centralnego autora projektu - architektura mesh jest natywnie P2P
  (flooding + tabela obecności między węzłami po radiu); `/server` i
  `client/desktop` to opcjonalny dodatek dla kogoś z dostępem do internetu, nie
  wymóg do działania sieci.
- [x] Pabianice OS (dystrybucja do stawiania własnych serwerów kanałów) dostępna
  pod koniec tej fazy - zbudowana, przetestowana (konta/role/kanały/uprawnienia
  per-kanał na czytanie i pisanie, panel admina), patrz `pabianice-os/README.md`.

## Co faktycznie blokuje wejście w Fazę 2

Strona software'owa obu faz jest gotowa i zweryfikowana (CI zielone na każdym
push - `firmware-ci.yml`, `server-ci.yml`, `pabianice-os-ci.yml`,
`client-desktop-ci.yml`; realne testy end-to-end firmware/crypto/server/klientów
opisane w commitach i README poszczególnych modułów). Zostają dwa punkty Fazy 1,
oba fizyczne, oba poza tym, co agent prowadzący repo może samodzielnie dowieźć:

1. Zbudować 3-5 fizycznych prototypów.
2. Zmierzyć realny zasięg radiowy w Pabianicach.

Dopiero po tych dwóch punktach (i decyzji właściciela projektu o faktycznym
udostępnieniu) przejście do Fazy 2 jest w pełni uzasadnione zgodnie z rozdz. 11
planu - kod i dokumentacja są na to gotowe już teraz.

## Co jest już przetestowane (dowód, nie deklaracja)

- Firmware: routing mesh, szyfrowanie, most BLE (`components/ble`) - kompilacja
  zweryfikowana w CI na docelowym `esp32s3` (ESP-IDF v5.2).
- Crypto (`components/crypto`, współdzielone z `client/desktop`): X3DH + Double
  Ratchet, dogenerowywanie one-time prekeys po wyczerpaniu puli, poprawna obsługa
  bundla bez one-time prekey - wszystko z realnymi testami end-to-end (dwie/trzy
  osobne tożsamości, prawdziwy `/server`, weryfikacja że wiadomości faktycznie się
  odszyfrowują).
- `/server`: testy integracyjne na tymczasowej bazie (`sqlx::test`), rate-limiting,
  blocklist, retencja.
- `pabianice-os`: konta/role, kategorie/kanały, uprawnienia per-kanał (czytanie i
  pisanie), panel admina - testy jednostkowe/integracyjne plus weryfikacja w
  przeglądarce (Playwright).
- `client/desktop`, `client/web-ble`: realne uruchomienia przeciw żywemu
  `/server`, przegląd zgodności API dla mostu BLE.
