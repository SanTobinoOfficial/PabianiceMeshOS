# Pabianice Comms

Prywatna, zdecentralizowana sieć komunikacji tekstowej działająca na fali radiowej
(LoRa, pasmo 868 MHz), niezależna od operatorów komórkowych i internetu.
Urządzenia (ESP32-S3 + moduł radiowy SX1262) tworzą sieć kratową (mesh) — wiadomość
wędruje skok po skoku przez węzły należące do różnych ludzi, aż dotrze do adresata.
Całość ma być szyfrowana end-to-end (Signal Protocol), tak żeby żaden pośredniczący
węzeł ani opcjonalny serwer centralny nie miał technicznej możliwości odczytania treści.

Projekt hobbystyczny, budowany wieczorami, w Pabianicach. Zobacz `docs/Pabianice_Comms_Plan_Pelny.docx`
po pełny plan techniczny i prawny (architektura, protokół routingu, model zagrożeń,
kalkulacja pokrycia miasta, fazy wdrożenia).

## Status

Wczesny etap — Faza 1 z planu wdrożenia (prototyp dla wąskiego grona znajomych,
zero zobowiązań prawnych). Gotowe: warstwa radiowa, routing mesh (flooding/TTL/dedup/
rate-limit), szyfrowanie end-to-end (Signal Protocol, `firmware/components/crypto`) oraz
szkielet serwera store-and-forward + katalogu kluczy (`/server`, Rust/Axum/Postgres/Redis).
Klucze na razie leżą w NVS, nie w module bezpiecznym — patrz `firmware/README.md`.
Serwer nie ma jeszcze autoryzacji panelu admina — patrz `server/README.md`.

**To jeszcze nie jest gotowy produkt.** Nie ma tu żadnej certyfikacji CE/RED, nie ma
gwarancji bezpieczeństwa. Jeśli budujesz własne urządzenie — sprawdź lokalne limity
mocy nadawania (ERP) i duty cycle dla pasma ISM w swoim kraju, to twoja odpowiedzialność.

## Jak to działa (w skrócie)

1. Nadawca szyfruje wiadomość kluczem publicznym odbiorcy (Signal Protocol — X3DH do
   ustanowienia sesji + Double Ratchet do szyfrowania kolejnych wiadomości).
2. Wiadomość trafia do najbliższego węzła w zasięgu radiowym, niezależnie czyją jest
   własnością.
3. Węzeł sprawdza lokalną tabelę obecności — czy odbiorca był ostatnio widziany w jego
   zasięgu. Jeśli tak, dostarcza bezpośrednio. Jeśli nie, rozgłasza dalej (flooding)
   z licznikiem TTL ograniczającym liczbę skoków.
4. Każdy węzeł po drodze cache'uje wiadomość na wypadek, gdyby odbiorca pojawił się
   w jego zasięgu później (store-and-forward, sieć DTN).
5. Opcjonalnie, po ~24h bez dostarczenia, węzeł z dostępem do internetu może wypchnąć
   zaszyfrowaną (nadal nieczytelną dla serwera) wiadomość do serwera centralnego.

Model zaufania jest zero-trust: nikt — łącznie z administratorem ewentualnego serwera
centralnego — nie musi być obdarzony zaufaniem co do treści komunikacji. To ten sam
model co w Signalu i Meshtastic.

## Struktura repo

- `firmware/` — kod ESP32-S3 (ESP-IDF, C). Warstwy: HAL/board, radio (SX1262 przez SPI),
  mesh/routing (flooding + TTL, tabela obecności, dedup), crypto (Signal Protocol przez
  libsignal-protocol-c) i warstwa aplikacyjna.
- `server/` — magazyn wiadomości store-and-forward + katalog kluczy publicznych
  (dochodzi w kroku 3).
- `client/` — szkielet aplikacji mobilnej, na razie tylko interfejs BLE do urządzenia.
- `docs/` — dokumentacja protokołu (`docs/protocol.md` — format ramki, typy pakietów,
  routing/TTL/dedup, rate-limiting, znane ograniczenia), pełny plan projektu.
- `hardware/` — schematy, BOM, pliki STL obudowy.
- `tutoriale/` — instrukcje krok po kroku dla ludzi, którzy chcą sami zbudować węzeł.
- `pabianice-os/` — (docelowo, po ustabilizowaniu reszty) dystrybucja Linux do
  self-hostingu własnego serwera z kanałami i panelem admina, z opcjonalną,
  domyślnie wyłączoną federacją z siecią główną.

## Plan wdrożenia

Projekt idzie fazami, każda ma być w pełni legalna sama w sobie:

1. **Prototyp** dla wąskiego grona znajomych — bez rejestracji, bez CE, bez RODO.
2. **Open source** — publikacja kodu, sieć rośnie organicznie w modelu P2P.
3. **Tryb scentralizowany (opt-in)** — dla chętnych, z pełną zgodnością RODO
   (co dzięki E2E encryption i tak sprowadza się do minimalnego zakresu danych).
4. **Sprzedaż gotowych zestawów** — dopiero po pełnej certyfikacji CE/RED i rejestracji
   działalności.

Szczegóły (w tym cała analiza prawna UKE/RED/RODO/CEIDG) w `docs/Pabianice_Comms_Plan_Pelny.docx`.
Szablony regulaminu i polityki prywatności pod Fazę 3 (jeszcze nie obowiązują) —
`docs/legal/`.

## Prowadzenie projektu

Project lead / osoba (a właściwie agent) prowadząca repo: Claude, agent AI działający
pod nadzorem właściciela repozytorium. To on pisze i przegląda kod, akceptuje commity
i pilnuje spójności architektury między poszczególnymi modułami.

## Budowanie

Instrukcje budowania firmware i montażu sprzętu: patrz `tutoriale/` i `firmware/README.md`.

## Licencja

GNU GPLv3 (`LICENSE`) - ten sam model co Meshtastic, do którego projekt się często
odwołuje: kod zostaje otwarty, także w formach pochodnych. Chcesz pomóc? Zobacz
`CONTRIBUTING.md`. Zasady społeczności: `CODE_OF_CONDUCT.md`.

## Strona i community

Strona projektu: **https://santobinoofficial.github.io/PabianiceMeshOS/** (źródło w
`website/`, deploy automatyczny na branch `gh-pages` przy każdym pushu).
Pytania, pomysły, pokazywanie zbudowanych węzłów →
[GitHub Discussions](https://github.com/SanTobinoOfficial/PabianiceMeshOS/discussions).
