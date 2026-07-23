# Pabianice OS

Dystrybucja do samodzielnego hostingu własnego serwera z kanałami tekstowymi,
rolami i panelem administracyjnym - analogicznie funkcjonalnie do serwera Discorda,
ale z opcjonalną (domyślnie wyłączoną) federacją z siecią główną Pabianice Comms
przez radio mesh albo internet. Rozdz. 10.3-10.4 pełnego planu w `docs/`.

To jest osobny produkt od `/server` (który jest DM store-and-forward + katalog
kluczy dla głównej sieci mesh, patrz `server/README.md`) - operator stawia to u
siebie dla swojej społeczności, niezależnie od głównej sieci projektu.

Instrukcja krok po kroku (instalacja + pierwsze użycie panelu):
[`tutoriale/04-stawianie-wlasnego-serwera.md`](../tutoriale/04-stawianie-wlasnego-serwera.md),
[`tutoriale/05-pierwsze-kroki-w-panelu.md`](../tutoriale/05-pierwsze-kroki-w-panelu.md).

## Stan na teraz

Działający szkielet - konta, role, kanały, wiadomości, panel admina, wszystko
realnie skompilowane i przetestowane (`cargo test`, `cargo clippy -D warnings`):

- **Konta i role** (`server/src/roles.rs`, `server/src/auth.rs`) - trzy poziomy
  (member/moderator/admin), hasła hashowane Argon2 (min. 8 znaków, wymuszane w
  `hash_password`), sesje jako losowy bearer token, w bazie trzymany tylko jako
  SHA-256 (`sessions.token_hash`, migracja 0003) - wyciek bazy/backupu nie daje
  gotowego dostępu do żadnej sesji. `POST /v1/auth/logout` faktycznie usuwa sesję
  z bazy, nie tylko czyści `localStorage` po stronie panelu. Brak publicznej
  samorejestracji - konta zakłada administrator (`POST /v1/users`), pierwsze
  konto powstaje automatycznie przy pierwszym starcie z
  `ADMIN_BOOTSTRAP_USERNAME`/`ADMIN_BOOTSTRAP_PASSWORD`. Zapomniane hasło resetuje
  admin (`PUT /v1/users/:id/password`) - jedyna ścieżka odzyskania konta, bo nie ma
  tu wysyłki maili.
- **Kategorie i kanały** (`server/src/routes/channels.rs`) - płaska struktura,
  tworzenie/usuwanie wymaga roli admin.
- **Wiadomości kanałowe** (`server/src/routes/messages.rs`) - pisanie wymaga
  minimalnej roli ustawionej per-kanał (`channels.min_post_role`, domyślnie
  member, admin może podnieść np. kanał ogłoszeń do moderator/admin przez
  `PUT /v1/channels/:id/min-post-role`). *Czytanie* ma analogiczny, niezależny
  próg (`channels.min_read_role`, `PUT /v1/channels/:id/min-read-role`) - kanał
  poniżej progu roli użytkownika jest dla niego całkowicie niewidoczny (znika z
  `GET /v1/channels`, nie tylko blokuje pisanie), domyślnie też `member` więc
  zero zmiany zachowania dopóki operator sam nie podniesie progu.
- **Ustawienia serwera** (`server/src/routes/admin.rs`, `server/src/retention.rs`) -
  retencja wiadomości (dni, 0 = bez limitu, niezależna od retencji `/server`,
  faktycznie egzekwowana co godzinę w tle) i przełącznik federacji - **sama
  flaga, nie działający protokół**, patrz niżej.
- **Panel admina** (`admin-panel/`) - statyczny HTML/CSS/vanilla JS, zero
  zależności zewnętrznych, serwowany z tego samego procesu co API
  (`tower-http::ServeDir`, patrz `server/src/routes/mod.rs`) - jeden port,
  zero CORS. Świadome odejście od rekomendacji "React" z rozdz. 10.2/10.4.4
  planu, w duchu reszty repo (`website/` też jest zero-dependency).
- **Instalator** (`install.sh`) - Debian/Ubuntu: pakiety systemowe, Postgres,
  Rust jeśli brakuje, build release, systemd unit, guided prompt na konto
  administratora (analogicznie do modelu YunoHost z rozdz. 10.3 planu).

## WAŻNA różnica modelu zaufania względem `/server`

DM w głównej sieci mesh (`/server`) to zawsze ciphertext Signal Protocol -
serwer nigdy nie widzi treści (zero-trust, rozdz. 4.3 planu). **Kanały tutaj
NIE są (jeszcze) szyfrowane end-to-end** - grupowe szyfrowanie (coś w rodzaju
Signal Sender Keys) to osobny, znacznie większy kawałek kryptografii, którego
`libsignal-protocol-c` używany w firmware nie dostarcza gotowego, i który nie
był jeszcze projektowany. Model zaufania kanałów jest więc na razie taki jak
Discord/Matrix/Slack: **operator serwera widzi treść wiadomości na swoim
serwerze**. To świadoma, udokumentowana różnica, nie przeoczenie - jeśli
kiedyś dojdzie grupowe E2E, to osobny, duży projekt.

Kolumna `identity_pub` w tabeli `users` istnieje (ta sama tożsamość kryptograficzna
co w mesh, rozdz. 10.4.1 planu), ale nic jeszcze jej realnie nie używa poza
przechowaniem - to zadel pod przyszłą integrację, nie działająca funkcja.

## Uruchomienie lokalnie (dev)

```bash
cd pabianice-os/server
cp .env.example .env
docker compose up -d          # Postgres na porcie 5433 (inny niz /server, zeby oba dzialaly naraz)
cargo run                     # migracje leca automatycznie, potem bootstrap pierwszego admina
```

Panel dostępny pod `http://localhost:8081/` (serwowany z `../admin-panel`, patrz
`ADMIN_PANEL_DIR` w `.env.example`).

## Instalacja produkcyjna (Debian/Ubuntu)

```bash
sudo ./install.sh
```

Pyta o konto administratora, port i (opcjonalnie) domenę. Instaluje Postgresa i
Rusta jeśli brakuje, buduje serwer, stawia systemd unit (`pabianice-os.service`)
i odpala go. Jeśli podasz domenę - stawia też Caddy jako reverse proxy z
automatycznym TLS (Let's Encrypt) i przełącza serwer na nasłuch tylko po
`127.0.0.1` (Caddy jest wtedy jedynym punktem wejścia z zewnątrz). Bez domeny
panel działa po zwykłym HTTP - dobre do testu w zaufanym LAN, nie do wystawienia
publicznie (hasło przy logowaniu leciałoby jawnym tekstem). Alpine Linux z planu
(rozdz. 10.3) na razie nieobsłużone - inny menedżer pakietów i init, do dopisania
jeśli ktoś tego faktycznie potrzebuje.

## Federacja - status

Rozdz. 10.4.2-10.4.3 planu opisuje model: każdy serwer w pełni samodzielny,
federacja z siecią główną opcjonalna i domyślnie wyłączona, z jasnym ekranem
zgody przy włączaniu (konsekwencje RODO - włączenie robi z administratora
głównej sieci współadministratora danych w rozumieniu RODO dla tego zakresu).

W tym szkielecie `federation_enabled` to tylko kolumna w `server_settings` -
**żaden protokół synchronizacji między serwerami nie jest zaimplementowany**.
Włączenie przełącznika w panelu niczego jeszcze funkcjonalnie nie zmienia.
Zaprojektowanie samego protokołu federacji (format wymiany, uwierzytelnianie
serwer-serwer, ekran zgody z konsekwencjami RODO) to osobny, spory kawałek
pracy, celowo odłożony - fundament (konta/role/kanały) musiał powstać pierwszy.

## Czego tu jeszcze brakuje

- Protokołu federacji (patrz wyżej) - na razie tylko flaga w bazie.
- Grupowego szyfrowania E2E kanałów (patrz "różnica modelu zaufania" wyżej).
- Realnej integracji `identity_pub` z resztą systemu (na razie tylko kolumna).
- Wsparcia Alpine Linux w `install.sh`.

## Endpointy (v1)

| Metoda | Ścieżka | Rola |
|---|---|---|
| POST | `/v1/auth/login` | - |
| POST | `/v1/auth/logout` | dowolny zalogowany (usuwa własną sesję) |
| GET/POST | `/v1/users` | moderator (GET) / admin (POST) |
| PUT | `/v1/users/:id/role` | admin |
| PUT | `/v1/users/:id/password` | admin (reset hasła, kończy wszystkie sesje danego usera) |
| GET/POST | `/v1/categories` | dowolny zalogowany (GET) / admin (POST) |
| GET/POST | `/v1/channels` | próg `min_read_role` (GET, kanały poniżej progu znikają z listy) / admin (POST) |
| DELETE | `/v1/channels/:id` | admin |
| PUT | `/v1/channels/:id/min-post-role` | admin |
| PUT | `/v1/channels/:id/min-read-role` | admin |
| GET/POST | `/v1/channels/:id/messages` | próg `min_read_role` (GET) / próg `min_post_role` (POST) |
| GET/PUT | `/v1/admin/settings` | moderator (GET) / admin (PUT) |

## Testy

`cargo test` w `pabianice-os/server` uruchamia zarówno czystą logikę (role,
hashowanie haseł) jak i realne testy integracyjne na tymczasowej bazie
(`#[sqlx::test]` - login/logout, tworzenie kont, wymuszanie `min_post_role`).
Wymaga zmiennej `DATABASE_URL` wskazującej na dowolny Postgres z uprawnieniem
tworzenia baz (sqlx sam tworzy i czyści tymczasową bazę per test) - w CI
(`pabianice-os-ci.yml`) to kontener `postgres:16-alpine` jako `services`.
