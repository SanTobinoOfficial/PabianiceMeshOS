# 5. Pierwsze kroki w panelu Pabianice OS

Zakładam, że masz już postawiony serwer (patrz
[4. Stawianie własnego serwera](04-stawianie-wlasnego-serwera.md)) i widzisz ekran
logowania w przeglądarce.

## Logowanie

Wpisz nazwę użytkownika i hasło administratora, które ustawiłeś przy instalacji
(albo w `.env` przy lokalnym teście), i kliknij "Zaloguj". Zobaczysz pasek na górze
z Twoją nazwą i rolą (`admin`), a po lewej pusty panel kanałów - to normalne, jeszcze
nic nie utworzyłeś.

## Tworzenie kategorii i kanałów

Przyciski `+ kanał` i `+ kategoria` widoczne są tylko dla roli `admin`.

1. Kliknij **`+ kategoria`**, wpisz nazwę (np. "Ogólne") - to tylko sposób grupowania
   kanałów na liście, nic więcej.
2. Kliknij **`+ kanał`**, wpisz nazwę kanału. Jeśli masz już przynajmniej jedną
   kategorię, panel zapyta do której kategorii go przypisać - wpisz jej nazwę dokładnie
   tak jak w podpowiedzi, albo zostaw puste, żeby kanał nie należał do żadnej. Ostatnie
   pytanie to temat kanału (`topic`) - też opcjonalne, Enter = brak.

Kanał pojawi się na liście po lewej, pogrupowany pod nazwą kategorii. Kliknij na niego,
żeby wejść i napisać pierwszą wiadomość - pole na dole ekranu, przycisk "Wyślij". Admin
widzi tam też przycisk **"Edytuj temat"**, gdyby trzeba było go zmienić albo dodać po
fakcie (pusta wartość = usuwa temat).

**Uwaga:** panel nie ma jeszcze sposobu na zmianę kolejności kanałów (`position`) -
to da się zrobić tylko bezpośrednio przez API, np.:

```bash
curl -X POST http://TWOJ-SERWER/v1/channels \
  -H "Authorization: Bearer TWOJ_TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"name": "ogloszenia", "position": 5}'
```

(Token dostajesz z odpowiedzi `POST /v1/auth/login` - pełna lista endpointów jest w
`pabianice-os/README.md`.)

## Dodawanie osób i zarządzanie rolami

Kliknij **"Użytkownicy"** (widoczne tylko dla admina) - zobaczysz listę kont i
formularz **"+ dodaj użytkownika"** (nazwa, hasło min. 8 znaków, rola) nad tabelą.
Rolę każdego istniejącego konta zmieniasz od razu z listy rozwijanej w kolumnie
"Rola" - zmiana zapisuje się natychmiast po wyborze, bez dodatkowego potwierdzania.

Role to `member` (domyślna, może pisać tam gdzie próg kanału na to pozwala),
`moderator` (dodatkowo widzi listę użytkowników i ustawienia serwera) i `admin`
(wszystko, w tym tworzenie/kasowanie kanałów i zmiana ról).

To samo da się zrobić przez API, przydatne np. do skryptów zakładających wiele
kont naraz:

```bash
curl -X POST http://TWOJ-SERWER/v1/users \
  -H "Authorization: Bearer TWOJ_TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"username": "kolega", "password": "jakies-haslo-min-8-znakow", "role": "member"}'
```

Jeśli ktoś zapomni hasła - w tabeli użytkowników przy każdym koncie jest przycisk
**"Resetuj hasło"**. Wpisujesz nowe hasło (min. 8 znaków), zatwierdzasz - stare hasło
przestaje działać i wszystkie dotychczasowe sesje tej osoby są od razu kończone.

### Kto może pisać na którym kanale

Każdy kanał ma próg minimalnej roli do pisania (`min_post_role`, domyślnie `member` -
czyli każdy zalogowany). Żeby np. zrobić kanał ogłoszeń, na którym piszą tylko
moderatorzy:

```bash
curl -X PUT http://TWOJ-SERWER/v1/channels/ID_KANALU/min-post-role \
  -H "Authorization: Bearer TWOJ_TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"min_post_role": "moderator"}'
```

(`ID_KANALU` znajdziesz w odpowiedzi `GET /v1/channels`.) To ogranicza tylko *pisanie*.

### Kto może w ogóle widzieć dany kanał

Osobny, niezależny próg (`min_read_role`, domyślnie też `member`) kontroluje
*widoczność* - kanał poniżej progu roli danego użytkownika znika mu całkowicie z
listy (nie pojawia się w panelu, nie da się go otworzyć nawet znając jego ID).
Przydatne np. do kanału dla samych moderatorów/adminów, o którym zwykli
użytkownicy nie powinni nawet wiedzieć:

```bash
curl -X PUT http://TWOJ-SERWER/v1/channels/ID_KANALU/min-read-role \
  -H "Authorization: Bearer TWOJ_TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"min_read_role": "moderator"}'
```

Ustawienie `min_read_role` wyżej niż `min_post_role` nie ma sensu (nikt by nie
widział kanału, na który niby może pisać) - serwer tego nie pilnuje, to po stronie
operatora, żeby ustawić oba progi sensownie.

## Ustawienia serwera

Kliknij **"Ustawienia serwera"** (widoczne dla moderatora i admina, ale zmieniać może
tylko admin):

- **Retencja wiadomości** - po ilu dniach stare wiadomości kanałowe mają być
  automatycznie usuwane. `0` = bez limitu (nic nie usuwa się automatycznie).
  Sprawdzane i egzekwowane w tle co godzinę.
- **Federacja włączona** - checkbox, ale **na razie to tylko zapisana decyzja, nie
  działająca funkcja**. Docelowo (patrz plan projektu) ma pozwolić Twojemu serwerowi
  wymieniać wiadomości z siecią główną albo z innymi serwerami Pabianice OS - sam
  protokół tej wymiany jeszcze nie istnieje. Zaznaczenie tego pola dzisiaj niczego
  funkcjonalnie nie zmienia.

## Wylogowanie

Przycisk "Wyloguj" w prawym górnym rogu faktycznie kończy sesję po stronie serwera
(nie tylko czyści dane w przeglądarce) - jeśli logowałeś się na wspólnym komputerze,
możesz być spokojny, że token przestaje działać.

## Co dalej

To wciąż wczesny szkielet - pełna lista tego, czego brakuje (uprawnienia na czytanie
kanałów, federacja, grupowe szyfrowanie E2E, wsparcie Alpine Linux w instalatorze) jest
w `pabianice-os/README.md`. Jeśli czegoś brakuje Ci najbardziej - zgłoś to jako issue
albo w Discussions repozytorium.
