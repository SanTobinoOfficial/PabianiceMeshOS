# Dokumenty prawne

Dwa szablony, przygotowane z wyprzedzeniem względem tego, kiedy będą faktycznie
potrzebne w harmonogramie projektu:

- [`polityka-prywatnosci.md`](polityka-prywatnosci.md)
- [`regulamin.md`](regulamin.md)

## Kiedy to w ogóle obowiązuje

Zgodnie z rozdz. 3 i 11 pełnego planu (`docs/Pabianice_Comms_Plan_Pelny.docx`):

- **Faza 1 (prototyp, wąskie grono znajomych)** i **Faza 2 (open source, sieć P2P bez
  serwera autora)** — te dokumenty **nie są prawnie wymagane**. RODO nie ma zastosowania
  do przetwarzania w ramach czynności czysto osobistych (art. 2 ust. 2 lit. c RODO),
  a bez sprzedaży i bez centralnego serwera dla obcych osób nie ma też obowiązków
  z prawa gospodarczego.
- **Faza 3 (opcjonalny tryb scentralizowany, opt-in)** — od tego momentu administrator
  serwera głównego staje się administratorem danych osobowych w rozumieniu RODO wobec
  osób spoza własnego, osobiście znanego grona. Tu te dokumenty zaczynają być potrzebne.
- **Operatorzy niezależnych serwerów na Pabianice OS (rozdz. 10.3-10.4)** — każdy taki
  operator jest odrębnym administratorem danych swoich własnych użytkowników i powinien
  wystawić **własną** politykę prywatności/regulamin (te tutaj mogą posłużyć jako punkt
  wyjścia do adaptacji, nie jako gotowe do podpięcia pod cudzy serwer).

## Zanim to pójdzie na produkcję

Te dokumenty to robocze szablony przygotowane na podstawie ogólnodostępnej wiedzy
o RODO i analizy prawnej z rozdz. 3 planu - **nie są poradą prawną**. Przed
faktycznym uruchomieniem Fazy 3 (a już na pewno przed Fazą 4 - sprzedaż) skonsultuj je
z prawnikiem specjalizującym się w RODO i prawie telekomunikacyjnym, dopasowując
zapisy do rzeczywistej architektury wdrożenia w tym momencie (mogła się zmienić
względem tego, co jest opisane tutaj).
