# Design system strony (website/)

Fuzja trzech języków, każdy wnosi coś innego, nie wszystkie tokeny na raz:

- **OpenCode** - głos: terminal-native monospace, ASCII-owe znaczniki
  `[+]`/`[x]`, mockup terminala z prawdziwym logiem.
- **Apple** - struktura: naprzemienne pełnoszerokie "kafle" jasne/ciemne
  (zmiana koloru = separator), pigułkowe przyciski, zdyscyplinowana skala
  typograficzna, dokładnie JEDEN cień zarezerwowany dla "produktu".
- **Notion** - kolor: fioletowy akcent zamiast niebieskiego Apple, jeden
  stonowany tag-chip (lawendowe tło + głęboki fiolet tekstu) do oznaczeń
  statusu.

Świadomie NIE wzięliśmy z Notion tęczowej palety kart (peach/rose/mint/sky/
lavender na raz) - to złamałoby zasadę "jeden akcent, jedna paleta" i
zamieniło spokojny system w bałagan. Jeden fiolet, użyty wszędzie
konsekwentnie, zamiast tęczy użytej wybiórczo. Też nie wzięliśmy fontu
Notion Sans ani kształtu przycisków Notion (rounded 8px) - jeden font
(monospace) i jeden kształt przycisku (pigułka) w całym systemie, bez
wyjątków.

Efekt: "co gdyby Apple i Notion projektowały razem stronę narzędzia dla
hakerów." Trzymaj się poniższych tokenów przy każdej kolejnej zmianie
treści zamiast wymyślać nowe.

## Fonty

Jeden font w całym systemie, monospace (OpenCode) - nie ma licencji na
Berkeley Mono, SF Pro ani Notion Sans, więc stos systemowy, bez ładowania
z zewnątrz:

```
ui-monospace, "SFMono-Regular", "IBM Plex Mono", "JetBrains Mono", Menlo,
Consolas, "Liberation Mono", "Courier New", monospace
```

Zero non-monospace znaków. Dyscyplina typograficzna jest z Apple: ciasny
letter-spacing na dużych rozmiarach, drabinka wag 400/600/700 (bez 500),
akapit na 17px (nie 16px) dla lepszego tempa czytania.

| Token | Rozmiar | Waga | Tracking | Użycie |
|---|---|---|---|---|
| hero-display | 42px | 700 | -0.01em | Nagłówek hero |
| display-lg | 30px | 700 | -0.005em | Nagłówki sekcji |
| lead | 19px | 400 | 0 | Podtytuł pod nagłówkiem |
| body | 17px | 400 | 0 | Akapity |
| body-strong | 17px | 600 | 0 | Wyróżnienia inline |
| button | 16px | 600 | 0 | Etykiety przycisków |
| caption | 14px | 400 | 0 | Metadane, stopka |

## Kolory

| Token | Wartość | Użycie |
|---|---|---|
| `--ink` | `#1d1d1f` | tekst główny na jasnym tle |
| `--body` | `#424245` | akapity |
| `--mute` | `#646262` | metadane |
| `--ash` | `#9a9898` | tekst drugorzędny na ciemnym tle |
| `--canvas` | `#fdfcfc` | jasna sekcja (naprzemiennie z parchment/dark) |
| `--parchment` | `#f5f5f7` | jasna sekcja, wariant (Apple, prawie identyczny z Notion `surface`) |
| `--surface-dark` | `#1d1d1f` | ciemna sekcja + nav (dokładnie jedna "pulsacja" rytmu) |
| `--accent` | `#5645d4` | JEDYNY kolor interaktywny (fiolet Notion) - wszystkie CTA, linki, podświetlenia w mockupie |
| `--accent-active` | `#4534b3` | stan wciśnięty przycisku primary |
| `--tag-bg` / `--tag-text` | `#e6e0f5` / `#391c57` | JEDYNY tag-chip (status "w budowie"), lawendowy ton z rodziny akcentu |
| `--success` | `#30d158` | tylko podświetlenie "OK"/status WEWNĄTRZ mockupu terminala |

## Zasady, których nie wolno złamać

- Jeden font wszędzie, zero sans-serif, zero kursywy.
- Jeden akcent (`--accent`, fiolet) na WSZYSTKICH CTA i linkach - żadnego
  drugiego koloru na przyciskach czy odnośnikach. Tag-chip to jedyny
  dodatkowy ton, i jest z tej samej rodziny (lawenda = jasny fiolet).
- Zero tęczowych kart. Jeśli kiedyś skusi Cię dodać kolejny "tinted card" -
  nie, chyba że to dokładnie ten sam odcień co `--accent`/`--tag-bg`.
- Kształty: przyciski = pigułka (`rounded: 9999px`), sekcje/kontenery = ostre
  rogi (0px), mały tag-chip = 8px. Nic pomiędzy - to udokumentowany wyjątek
  od "jeden promień", nie przypadkowe mieszanie.
- Sekcje to pełnoszerokie "kafle" na przemian jasne (`canvas`/`parchment`) i
  ciemne (`surface-dark`) - zmiana koloru JEST separatorem, żadnych
  dekoracyjnych linii między sekcjami.
- Dokładnie JEDEN cień w całym systemie: miękki, tłumiony (nie czarny),
  wyłącznie pod kartą mockupu terminala (to nasz "produkt" w sensie Apple).
  Nigdzie indziej cieni.
- ASCII-owe znaczniki `[+]` / `[x]` / `[ ]` jako markery list (OpenCode) -
  jedyna "ikonografia" w systemie, żadnych SVG, żadnych ilustracji sticky-note.
- Log w mockupie terminala to prawdziwe linie z `tutoriale/03-...md` / logów
  firmware, nie wymyślony fake output.
- Liczby w sekcji statystyk to realne dane z `docs/Pabianice_Comms_Plan_Pelny.docx`
  - nie fabrykujemy precyzyjnych metryk.
- Zero em-dash (`—`/`–`) w treści. Tylko zwykły myślnik `-`. Maks. jedna
  kropka pośrodkowa (`·`) na linię.
- 80px odstęp wewnątrz sekcji (Apple `spacing.section`), bez dodatkowych
  dekoracyjnych dividerów.

## Kiedy to aktualizować

Za każdą istotną zmianę w projekcie (nowa faza, nowy krok firmware/servera,
zmiana statusu) - zaktualizuj `index.html` żeby odzwierciedlał aktualny stan,
zachowując powyższe tokeny. Nie dodawaj nowych komponentów bez wyraźnej potrzeby.
