# server

Magazyn wiadomości store-and-forward dla odbiorców chwilowo offline + katalog kluczy
publicznych (rozdz. 10 pełnego planu). Rust + Axum, PostgreSQL, Redis. Serwer widzi
wyłącznie zaszyfrowane blob-y (ciphertext Signal Protocol) i hashe adresów węzłów -
nie ma technicznej możliwości odczytania treści (model zaufania, rozdz. 4.3).

## Stan na teraz

Szkielet - trzy funkcje z rozdz. 10.1 planu:

1. **Katalog kluczy publicznych** (`/v1/keys`) - węzeł publikuje swój bundle (identity key,
   signed prekey, one-time prekeys), inny węzeł go pobiera żeby zacząć sesję X3DH, dokładnie
   ten sam format co bezpośrednia wymiana `PKT_TYPE_KEY_BUNDLE` w `firmware/components/mesh`,
   tylko przez serwer zamiast wprost między sąsiadami.
2. **Magazyn wiadomości** (`/v1/messages`) - store-and-forward, domyślna retencja 90 dni
   (rozdz. 10.2 planu), Postgres jako trwały storage + Redis pub/sub jako "nudge" dla
   ewentualnego gatewaya trzymającego otwarte połączenie (opcjonalny - jak Redis nie
   działa, polling przez Postgres i tak znajdzie wiadomość). Potwierdzenie dostarczenia
   (`DELETE .../id/:id`) wymaga podania własnego `node_id` w ścieżce - działa tylko jeśli
   zgadza się z `dst_id` tej wiadomości, więc sama znajomość UUID już nie wystarcza.
   Rate-limiting per `src_id` (Redis, 20 wiadomości/60s, ten sam rząd wielkości co
   `RATE_LIMIT_MAX_PER_WIN` w `mesh.c` firmware) - fail-open jeśli Redis akurat nie działa.
3. **Lista zablokowanych HWID** (`/v1/blocklist`, rozdz. 7.3.1 planu) - chronione
   bearer tokenem (`ADMIN_TOKEN` w env, patrz `.env.example` i `src/auth.rs`). Pełny
   panel webowy z kontami i 2FA to Pabianice OS (rozdz. 10.4), osobny, późniejszy etap -
   na razie to jeden statyczny token do jednego operatora serwera.

## Uruchomienie lokalnie

```bash
cd server
cp .env.example .env
docker compose up -d          # Postgres + Redis do dev
cargo run                     # migracje leca automatycznie przy starcie
```

Serwer nasłuchuje na `:8080` (konfigurowalne przez `BIND_ADDR`).

## Endpointy (v1)

| Metoda | Ścieżka | Co robi |
|---|---|---|
| PUT | `/v1/keys/:node_id` | publikacja/odświeżenie własnego bundla kluczy |
| GET | `/v1/keys/:node_id` | pobranie bundla peera (zużywa jeden one-time prekey) |
| POST | `/v1/messages` | wypchnięcie zaszyfrowanej wiadomości do kolejki |
| GET | `/v1/messages/:node_id` | odbiór wiadomości oczekujących na dany węzeł |
| DELETE | `/v1/messages/:node_id/id/:id` | potwierdzenie dostarczenia (tylko jeśli `node_id` = `dst_id`) |
| GET/PUT/DELETE | `/v1/blocklist[/:hwid]` | lista/blokowanie/odblokowanie HWID |

`node_id` i `hwid` w URL-ach to hex (16 znaków = 8 bajtów, ten sam format co
`PKT_NODE_ID_LEN` w firmware). Pola binarne w body JSON (klucze, ciphertext) też jako hex.

## Testy

`cargo test` odpala zarówno czystą logikę (`parse_node_id`) jak i realne testy
integracyjne na tymczasowej bazie (`#[sqlx::test]`) - roundtrip publikacji/pobrania
kluczy, zużywanie one-time prekeya dokładnie raz, wymuszanie `node_id` przy ack,
blokada nadawcy z blocklisty, przekroczenie rate-limitu. Wymaga `DATABASE_URL`
(Postgres z uprawnieniem tworzenia baz) i lokalnie dostępnego Redis pod
`redis://127.0.0.1:6379` (patrz `src/state.rs::test_state`).

## Czego tu jeszcze brakuje

- Panelu administracyjnego webowego z kontami i 2FA (Pabianice OS, rozdz. 10.4) - na
  razie tylko surowe REST API za jednym statycznym tokenem.

Uruchomienie tego serwera dla kogokolwiek poza wąskim gronem znajomych to Faza 3 planu -
od tego momentu zaczynają obowiązywać `docs/legal/regulamin.md` i
`docs/legal/polityka-prywatnosci.md` (na razie szablony robocze, patrz `docs/legal/README.md`).
