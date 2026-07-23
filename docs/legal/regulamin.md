# Regulamin — Pabianice Comms

> **Status: szablon roboczy, jeszcze nie obowiązuje.** Pisany pod Fazę 3 planu
> (opcjonalny tryb scentralizowany z serwerem głównym) — patrz `docs/legal/README.md`.
> W Fazie 1/2 (sieć lokalna P2P, projekt open source bez usługi świadczonej przez
> autora) ten regulamin nie ma zastosowania — obowiązuje tylko licencja open source
> kodu (`LICENSE` w repo) i ogólne przepisy prawa.

Ostatnia aktualizacja: *(uzupełnić przy wdrożeniu)*

## 1. Postanowienia ogólne

1.1. Niniejszy regulamin określa zasady korzystania z opcjonalnego, scentralizowanego
trybu synchronizacji Pabianice Comms (dalej: „Usługa"), prowadzonego przez
*(uzupełnić: administrator/operator)* (dalej: „Operator").

1.2. Pabianice Comms jest z założenia projektem open source. Sam firmware urządzeń
i oprogramowanie sieci mesh działają w pełni lokalnie, bez udziału Operatora i bez
konieczności akceptacji niniejszego regulaminu — dotyczy on wyłącznie korzystania
z opcjonalnego serwera centralnego prowadzonego przez Operatora.

1.3. Kod źródłowy projektu jest dostępny na licencji open source wskazanej w pliku
`LICENSE` repozytorium. Regulamin nie ogranicza praw wynikających z tej licencji.

## 2. Definicje

- **Sieć mesh** — zdecentralizowana sieć radiowa złożona z urządzeń (węzłów)
  należących do różnych, niezależnych od siebie osób.
- **Węzeł** — pojedyncze urządzenie nadawczo-odbiorcze pracujące w sieci mesh.
- **Usługa** — opcjonalna funkcja przechowywania i przekazywania zaszyfrowanych
  wiadomości (store-and-forward) oraz katalog kluczy publicznych, świadczone przez
  Operatora za pośrednictwem serwera centralnego.
- **Identyfikator** — skrót klucza publicznego użytkownika, używany do adresowania
  wiadomości; nie jest daną bezpośrednio identyfikującą użytkownika.

## 3. Warunki korzystania

3.1. Z Usługi może korzystać każdy posiadacz kompatybilnego urządzenia, który świadomie
włączył opcję synchronizacji z serwerem centralnym (opt-in). Włączenie tej opcji jest
równoznaczne z akceptacją niniejszego regulaminu i Polityki Prywatności.

3.2. Budowa, posiadanie i używanie urządzenia radiowego (węzła) odbywa się na wyłączną
odpowiedzialność użytkownika, w szczególności w zakresie zgodności z lokalnie
obowiązującymi przepisami dotyczącymi emisji radiowej (limity mocy ERP i duty cycle dla
pasma ISM, rozdz. 3.1 planu) oraz — jeśli dotyczy — zgodności urządzenia z dyrektywą RED
i oznakowaniem CE (rozdz. 3.2 planu).

3.3. Operator nie jest producentem ani sprzedawcą urządzeń użytkownika, chyba że
wyraźnie zaznaczono inaczej (np. w ramach Fazy 4 planu — sprzedaż certyfikowanych
zestawów). Operator odpowiada wyłącznie za działanie serwera centralnego.

## 4. Charakter sieci mesh i ograniczenia usługi

4.1. Węzły sieci mesh są własnością różnych, niezależnych od siebie osób. Operator nie
gwarantuje ciągłości działania, zasięgu ani dostępności żadnego konkretnego węzła
poza infrastrukturą serwera centralnego, którą sam prowadzi.

4.2. Dostarczenie wiadomości w sieci mesh odbywa się w modelu „best-effort" (sieć
opóźnieniotolerancyjna, DTN — rozdz. 5 planu). Operator nie gwarantuje czasu ani samego
faktu dostarczenia wiadomości — w szczególności gdy odbiorca pozostaje poza zasięgiem
sieci przez czas dłuższy niż okres retencji określony w Polityce Prywatności.

4.3. Operator nie ma technicznej możliwości odczytania treści wiadomości przesyłanych
przez Usługę (szyfrowanie end-to-end, rozdz. 6 planu) i nie ponosi odpowiedzialności
za treści przesyłane przez użytkowników.

## 5. Zasady korzystania i zakazane zachowania

5.1. Zabronione jest:

- celowe zalewanie sieci nadmiarowym ruchem (flooding) w sposób wykraczający poza
  normalne korzystanie z Usługi;
- podejmowanie prób ominięcia mechanizmów ograniczających nadużycia (rate-limiting,
  blokady identyfikatorów sprzętowych);
- wykorzystywanie Usługi do przesyłania treści niezgodnych z obowiązującym prawem;
- podszywanie się pod innego użytkownika lub węzeł sieci.

5.2. Ze względu na architekturę open source i model DIY (samodzielna budowa urządzeń,
rozdz. 7.3.3 planu) Operator zastrzega, że zablokowanie identyfikatora sprzętowego
w ramach Usługi nie stanowi trwałej bariery technicznej dla zdeterminowanego użytkownika
budującego nowe urządzenie z nowym identyfikatorem — jest to świadomy i nieunikniony
kompromis architektury, a nie błąd zabezpieczeń.

## 6. Ograniczenie odpowiedzialności

6.1. Usługa oraz oprogramowanie sieci mesh są dostarczane w modelu open source „tak jak
są" (as-is), bez gwarancji przydatności do konkretnego celu, w zakresie dopuszczalnym
przez obowiązujące przepisy prawa.

6.2. Operator nie ponosi odpowiedzialności za:

- szkody wynikłe z niedostarczenia, opóźnienia lub utraty wiadomości w sieci mesh;
- działania innych użytkowników lub operatorów niezależnych węzłów/serwerów;
- niezgodność urządzenia użytkownika z przepisami radiowymi lub dyrektywą RED —
  odpowiedzialność za budowę i eksploatację urządzenia spoczywa na jego budowniczym/
  właścicielu (pkt 3.2);
- skutki korzystania z niezależnych serwerów postawionych przez innych operatorów na
  dystrybucji Pabianice OS (rozdz. 10.3-10.4 planu) — każdy taki serwer jest prawnie
  odrębny od Operatora i od infrastruktury głównej.

## 7. Federacja z innymi serwerami

Jeśli operator niezależnego serwera (Pabianice OS) świadomie włączy opcjonalną
federację z siecią główną (rozdz. 10.4.2 planu), niniejszy regulamin i Polityka
Prywatności stosują się wyłącznie w zakresie danych faktycznie przepływających do
infrastruktury głównej w wyniku tej federacji. Zasady dotyczące pozostałych danych
określa regulamin danego serwera niezależnego, ustalany przez jego operatora.

## 8. Zmiany regulaminu

Operator zastrzega sobie prawo do zmiany regulaminu, w szczególności w związku ze
zmianą zakresu funkcjonalnego Usługi. O istotnych zmianach użytkownicy zostaną
poinformowani z odpowiednim wyprzedzeniem przed ich wejściem w życie. Dalsze korzystanie
z Usługi po wejściu w życie zmian oznacza ich akceptację.

## 9. Prawo właściwe i rozstrzyganie sporów

9.1. Do niniejszego regulaminu zastosowanie ma prawo polskie.

9.2. Spory wynikłe z korzystania z Usługi Operator dąży do rozwiązywania polubownie.
W przypadku braku porozumienia właściwy jest sąd zgodnie z obowiązującymi przepisami
o właściwości miejscowej.

## 10. Kontakt

*(uzupełnić: adres e-mail lub inny kanał kontaktowy do Operatora)*
