# Polityka prywatności — Pabianice Comms (serwer centralny, tryb scentralizowany)

> **Status: szablon roboczy, jeszcze nie obowiązuje.** Dotyczy Fazy 3 planu (opcjonalny
> tryb scentralizowany) — patrz `docs/legal/README.md`. Przed faktycznym wdrożeniem
> wymaga uzupełnienia danych administratora i przeglądu prawnego. Nie stosuje się do
> Fazy 1/2 (sieć lokalna P2P, bez serwera) — tam RODO nie ma zastosowania.

Ostatnia aktualizacja: *(uzupełnić przy wdrożeniu)*

## 1. Administrator danych

Administratorem danych osobowych przetwarzanych w związku z korzystaniem z opcjonalnego
trybu scentralizowanego Pabianice Comms jest:

*(uzupełnić: imię i nazwisko / nazwa podmiotu, adres do doręczeń, adres e-mail
kontaktowy — wymóg art. 13 ust. 1 lit. a RODO)*

Ta polityka dotyczy **wyłącznie** serwera głównego prowadzonego przez administratora
wskazanego wyżej. Jeśli korzystasz z niezależnego serwera postawionego przez kogoś
innego na dystrybucji Pabianice OS (rozdz. 10.3-10.4 planu), administratorem Twoich
danych jest operator tego serwera — poproś go o jego własną politykę prywatności.

## 2. Czego ta polityka NIE dotyczy

Jeśli korzystasz z Pabianice Comms wyłącznie w trybie lokalnym (P2P, sieć mesh bez
synchronizacji z jakimkolwiek serwerem) — nie przesyłasz żadnych danych do administratora
wskazanego w pkt 1, więc ta polityka Cię nie dotyczy. Twoje urządzenie komunikuje się
bezpośrednio z innymi węzłami w zasięgu radiowym, bez pośrednictwa naszej infrastruktury.

## 3. Jakie dane przetwarzamy i po co

Włączenie synchronizacji z serwerem centralnym jest **opcją, którą świadomie włączasz**
(opt-in) — nie jest domyślnie aktywna. Po jej włączeniu przetwarzamy:

| Kategoria danych | Cel przetwarzania | Podstawa prawna |
|---|---|---|
| Zaszyfrowane treści wiadomości (ciphertext end-to-end, nieczytelny dla nas) | Tymczasowe przechowanie do momentu dostarczenia odbiorcy będącemu offline (store-and-forward) | Zgoda (art. 6 ust. 1 lit. a RODO) — włączenie synchronizacji |
| Skrót identyfikatora publicznego nadawcy i odbiorcy (hash klucza publicznego) | Trasowanie wiadomości do właściwego odbiorcy | Zgoda / niezbędność do świadczenia usługi (art. 6 ust. 1 lit. a i b) |
| Znacznik czasu wiadomości | Zarządzanie retencją (automatyczne usuwanie po upływie okresu przechowywania) | Prawnie uzasadniony interes (art. 6 ust. 1 lit. f) — minimalizacja przechowywanych danych |
| Identyfikator sprzętowy urządzenia (HWID) | Blokowanie urządzeń nadużywających sieci (rozdz. 7.3 planu) | Prawnie uzasadniony interes — ochrona sieci przed nadużyciami |
| Adres IP (jeśli synchronizacja odbywa się przez internet) | Nawiązanie połączenia, ochrona przed nadużyciami na poziomie sieciowym | Prawnie uzasadniony interes |

**Nie przetwarzamy** Twojego imienia, nazwiska, numeru telefonu, adresu e-mail ani
żadnych innych danych identyfikujących Cię wprost — architektura systemu (rozdz. 4.3
planu) celowo tego nie wymaga. Nie mamy też technicznej możliwości odczytania treści
Twoich wiadomości — patrz pkt 7.

## 4. Okres przechowywania danych

Zaszyfrowane wiadomości przechowujemy przez *(uzupełnić — domyślnie proponowane w planie:
90 dni, rozdz. 10.2)* od momentu otrzymania, a następnie są automatycznie i trwale
usuwane niezależnie od tego, czy zostały dostarczone. Jeśli wiadomość zostanie dostarczona
odbiorcy wcześniej, jest usuwana z serwera niezwłocznie po potwierdzeniu dostarczenia.

Lista zablokowanych identyfikatorów sprzętowych (HWID) jest przechowywana do czasu
odwołania blokady przez administratora.

## 5. Komu przekazujemy dane

Nie sprzedajemy ani nie udostępniamy Twoich danych podmiotom trzecim w celach
marketingowych. Dane mogą być przetwarzane przez:

- dostawcę hostingu/infrastruktury serwerowej — wyłącznie jako podmiot przetwarzający
  na podstawie umowy powierzenia przetwarzania danych (art. 28 RODO);
- organy publiczne — wyłącznie gdy istnieje ku temu podstawa prawna (np. prawomocne
  żądanie sądu lub uprawnionego organu). Ze względu na szyfrowanie end-to-end nie mamy
  technicznej możliwości przekazania treści wiadomości nawet w takim przypadku —
  możemy przekazać co najwyżej metadane opisane w pkt 3.

Jeśli włączysz opcjonalną **federację** z innym serwerem (dotyczy operatorów Pabianice
OS, rozdz. 10.4.2-10.4.3 planu), część danych routingu zacznie przepływać także do
infrastruktury tego serwera — dzieje się to wyłącznie po Twojej świadomej zgodzie
wyrażonej przez osobny ekran zgody, nie przez ukryte ustawienie.

## 6. Twoje prawa

Zgodnie z RODO przysługuje Ci prawo do:

- dostępu do swoich danych (art. 15),
- sprostowania danych (art. 16),
- usunięcia danych / „bycia zapomnianym" (art. 17) — w praktyce: wyłączenia
  synchronizacji i żądania usunięcia oczekujących wiadomości powiązanych z Twoim
  identyfikatorem,
- ograniczenia przetwarzania (art. 18),
- przenoszenia danych (art. 20),
- sprzeciwu wobec przetwarzania opartego o prawnie uzasadniony interes (art. 21),
- wniesienia skargi do Prezesa Urzędu Ochrony Danych Osobowych (UODO), ul. Stawki 2,
  00-193 Warszawa, jeśli uznasz, że przetwarzanie narusza RODO.

Aby skorzystać z tych praw, skontaktuj się z administratorem — dane kontaktowe w pkt 1.
Ponieważ nie przechowujemy danych identyfikujących Cię wprost, prosimy o kontakt
z podaniem identyfikatora publicznego, którego dotyczy żądanie.

## 7. Szyfrowanie end-to-end i bezpieczeństwo

Treść Twoich wiadomości jest szyfrowana end-to-end (Signal Protocol — X3DH + Double
Ratchet, rozdz. 6 planu) na Twoim urządzeniu, zanim opuści je w jakiejkolwiek formie.
Administrator serwera centralnego przechowuje wyłącznie zaszyfrowane bloki danych i nie
posiada technicznej możliwości ich odczytania — nawet na żądanie. Znacząco ogranicza to
faktyczny zakres ryzyka związanego z przetwarzaniem i ułatwia realizację zasady
minimalizacji danych (art. 5 ust. 1 lit. c RODO).

Szyfrowanie end-to-end **nie ukrywa** metadanych połączeń — faktu komunikacji między
dwoma identyfikatorami, przybliżonego czasu i częstotliwości wymiany wiadomości ani
przybliżonego rozmiaru wiadomości. Jest to znane, udokumentowane ograniczenie
architektury (rozdz. 6.5 planu), nie luka w implementacji.

## 8. Zgłaszanie naruszeń ochrony danych

W przypadku wykrycia naruszenia ochrony danych osobowych mogącego skutkować ryzykiem
naruszenia praw lub wolności osób, których dane dotyczą, administrator zgłosi je do
UODO w ciągu 72 godzin od wykrycia (art. 33 RODO) oraz — jeśli ryzyko jest wysokie —
poinformuje osoby, których dane dotyczą (art. 34 RODO).

## 9. Zmiany polityki

Zastrzegamy sobie prawo do zmiany niniejszej polityki, w szczególności w związku ze
zmianą zakresu funkcjonalnego serwera. O istotnych zmianach poinformujemy z odpowiednim
wyprzedzeniem przed ich wejściem w życie.

## 10. Kontakt

*(uzupełnić: adres e-mail lub inny kanał kontaktowy do administratora w sprawach
ochrony danych)*
