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
   działa, polling przez Postgres i tak znajdzie wiadomość).
3. **Lista zablokowanych HWID** (`/v1/blocklist`, rozdz. 7.3.1 planu) - **bez autoryzacji
   na tym etapie**, nie wystawiać tego publicznie. Panel admina z 2FA (rozdz. 10.2) to
   osobny, późniejszy kawałek pracy.

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
| DELETE | `/v1/messages/id/:id` | potwierdzenie dostarczenia, usuwa z kolejki |
| GET/PUT/DELETE | `/v1/blocklist[/:hwid]` | lista/blokowanie/odblokowanie HWID |

`node_id` i `hwid` w URL-ach to hex (16 znaków = 8 bajtów, ten sam format co
`PKT_NODE_ID_LEN` w firmware). Pola binarne w body JSON (klucze, ciphertext) też jako hex.

## Czego tu jeszcze brakuje

- Autoryzacji na `/v1/blocklist` i na `DELETE /v1/messages/id/:id`.
- Rate-limitingu per node_id na `/v1/messages` (serwer na razie ufa, że to firmware
  już przefiltrował nadużycia lokalnie - rozdz. 7.2 planu; warto mieć obronę i tutaj).
- Panelu administracyjnego (rozdz. 10.2) - na razie tylko surowe REST API.
- Testów integracyjnych względem realnej bazy.

Uruchomienie tego serwera dla kogokolwiek poza wąskim gronem znajomych to Faza 3 planu -
od tego momentu zaczynają obowiązywać `docs/legal/regulamin.md` i
`docs/legal/polityka-prywatnosci.md` (na razie szablony robocze, patrz `docs/legal/README.md`).
