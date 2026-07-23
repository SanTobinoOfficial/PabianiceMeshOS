# 4. Stawianie własnego serwera (Pabianice OS)

Do tej pory tutoriale 1-3 dotyczyły głównej sieci mesh - budowy węzła i wysyłania
zaszyfrowanych wiadomości bezpośrednio między urządzeniami. Ten tutorial jest o czymś
innym: **Pabianice OS** (katalog `pabianice-os/` w repo) to osobny program, do
postawienia własnego serwera z kanałami tekstowymi dla swojej społeczności -
funkcjonalnie coś jak własny serwer Discorda, tylko że kod jest w pełni Twój.

## Czym to się różni od głównej sieci

Główny serwer (`/server`) to magazyn zaszyfrowanych wiadomości i katalog kluczy dla
sieci mesh - przechowuje dane (ciphertext, nie treść) dla węzłów, które akurat są
offline. Nie ma tam kont ani kanałów.

Pabianice OS to coś, co stawiasz **u siebie**, dla swojej grupy znajomych albo
społeczności - z kontami, rolami (member/moderator/admin) i kanałami tekstowymi
pogrupowanymi w kategorie. Możesz mieć serwer Pabianice OS bez żadnego związku z
siecią mesh - to w pełni samodzielny program. Docelowo (rozdz. 10.4 planu) będzie
też opcja, żeby taki serwer dogadywał się z siecią główną (federacja) albo nawet
tworzył własną, niezależną sieć - to dopiero zapowiedziane w planie, na razie sam
przełącznik istnieje, ale nic za nim jeszcze nie stoi (patrz `pabianice-os/README.md`,
sekcja "Federacja - status").

**Ważna różnica modelu zaufania:** wiadomości w kanałach na Twoim serwerze **nie są**
szyfrowane end-to-end tak jak DM-y w głównej sieci - Ty, jako operator, widzisz ich
treść na swoim serwerze (dokładnie tak jak na Discordzie czy Slacku). To świadoma
decyzja projektowa, nie przeoczenie - grupowe szyfrowanie E2E to osobny, duży kawałek
kryptografii, którego jeszcze nie ma.

## Wymagania

- Debian albo Ubuntu (instalator na inne dystrybucje na razie nie działa)
- Dostęp `root`/`sudo`
- Jeśli chcesz TLS (zalecane do czegokolwiek poza testem w LAN) - domena wskazująca
  na adres IP tej maszyny

## Szybki start lokalny (do testów, bez instalatora)

Jeśli chcesz tylko pomacać program na własnym komputerze, bez stawiania go na
serwerze:

```bash
cd pabianice-os/server
cp .env.example .env
docker compose up -d      # stawia Postgresa na porcie 5433
cargo run                 # migracje leca automatycznie, potem tworzy pierwsze konto admina
```

Domyślne dane logowania (z `.env.example`) to `admin` / `zmien-mnie` - zmień je w `.env`
zanim odpalisz, jeśli to coś więcej niż chwilowy test na własnym laptopie. Panel jest
dostępny pod `http://localhost:8081/`.

## Instalacja na serwerze (produkcyjnie)

Skopiuj cały katalog `pabianice-os/` na maszynę, na której chcesz postawić serwer
(albo sklonuj całe repo), i odpal instalator jako root:

```bash
sudo ./install.sh
```

Skrypt zapyta o:

1. **Nazwę użytkownika administratora** (domyślnie `admin`).
2. **Hasło administratora** - min. 8 znaków, inaczej serwer i tak je odrzuci przy
   pierwszym uruchomieniu.
3. **Port**, na którym ma nasłuchiwać serwer (domyślnie `8081`).
4. **Domenę** - to pytanie decyduje, czy dostaniesz automatyczny TLS.

Dalej sam:

- instaluje Postgresa i Rusta (jeśli ich nie ma),
- zakłada bazę danych z losowym hasłem,
- buduje serwer (`cargo build --release` - może chwilę potrwać na słabszej maszynie),
- stawia go jako usługę systemd (`pabianice-os.service`, `systemctl status pabianice-os`
  pokaże czy działa).

### Z domeną - automatyczny TLS

Jeśli podałeś domenę, instalator dodatkowo stawia [Caddy](https://caddyserver.com/)
jako reverse proxy z automatycznym certyfikatem TLS (Let's Encrypt) i przełącza sam
serwer Pabianice OS na nasłuch tylko po `127.0.0.1` - od zewnątrz widoczny jest
wyłącznie Caddy. Warunek: domena musi już wskazywać (rekord A/AAAA) na adres IP tej
maszyny, i porty 80/443 muszą być otwarte - inaczej Caddy nie dostanie certyfikatu.

### Bez domeny - zwykły HTTP

Jeśli zostawisz to pole puste, panel będzie dostępny po zwykłym `http://`. To wystarcza
do testu w zaufanej sieci lokalnej, ale **hasło przy logowaniu leci wtedy jawnym
tekstem** - nie wystawiaj tak skonfigurowanego serwera do internetu. Możesz zawsze
odpalić `install.sh` ponownie z domeną później, albo postawić własny reverse proxy.

## Sprawdzenie, że działa

Po zakończeniu instalator wypisze adres panelu i przypomni dane logowania. Otwórz ten
adres w przeglądarce - powinieneś zobaczyć ekran logowania Pabianice OS. Jeśli coś nie
działa:

```bash
sudo systemctl status pabianice-os     # czy usługa w ogóle wystartowała
sudo journalctl -u pabianice-os -n 50  # logi, zazwyczaj widac tam dlaczego nie
```

Dalej: [5. Pierwsze kroki w panelu](05-pierwsze-kroki-w-panelu.md) - jak stworzyć
kategorie, kanały, dodać znajomych i ustawić uprawnienia.
