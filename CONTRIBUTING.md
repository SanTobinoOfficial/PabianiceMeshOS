# Współpraca przy Pabianice Comms

Dzięki za zainteresowanie. Projekt jest wczesny (Faza 1/2 z planu wdrożenia), więc
zasady są proste i mało formalne - to się może zmienić wraz ze skalą.

## Zanim zaczniesz

- Przeczytaj `README.md` (struktura repo, status, fazy) i `docs/Pabianice_Comms_Plan_Pelny.docx`
  (pełny plan techniczny i prawny) - większość pytań "dlaczego tak, a nie inaczej"
  ma tam odpowiedź.
- Firmware wymaga ESP-IDF i realnego sprzętu (ESP32-S3 + SX1262) do pełnego przetestowania -
  patrz `firmware/README.md` i `tutoriale/`.

## Gdzie co zgłaszać

- **Pytania, pomysły, pokazanie zbudowanego węzła, wyniki testów zasięgu** →
  [GitHub Discussions](https://github.com/SanTobinoOfficial/PabianiceMeshOS/discussions).
- **Konkretne błędy i dopracowane propozycje zmian** → Issues (mamy szablony).
- **Kod** → Pull Request, patrz niżej.

## Pull requesty

1. Małe, logiczne commity z opisowymi komunikatami - nie jeden gigantyczny commit na cały PR.
2. Firmware: jeśli zmieniasz coś w warstwie radiowej (moc nadawania, częstotliwość,
   parametry duty cycle) - zaznacz to wyraźnie w opisie PR. To nie jest formalność,
   przekroczenie limitów ISM to realne ryzyko prawne dla każdego kto to wgra na żywy
   nadajnik (rozdz. 3.1 planu).
3. Nie musisz mieć fizycznego sprzętu, żeby pomóc - review kodu, dokumentacja, tutoriale,
   `/server` i `/client` da się w dużej mierze rozwijać bez LoRa pod ręką.
4. Styl kodu: patrz istniejące pliki. Ogólna zasada - kod ma wyglądać jak pisany przez
   człowieka wieczorami po pracy, nie jak wygenerowany. Komentarze tylko tam, gdzie
   coś faktycznie nieoczywiste (workaround sprzętowy, świadomy wybór wartości).

## Bezpieczeństwo

Jeśli znajdziesz realną lukę bezpieczeństwa (nie literówkę w komentarzu, tylko coś co
faktycznie osłabia szyfrowanie E2E albo pozwala na atak) - zanim opiszesz to publicznie
w issue, rozważ zgłoszenie prywatnie przez GitHub Security Advisories dla tego repo.
Kryptografia (Signal Protocol) jest jawna i audytowalna celowo (rozdz. 6.4 planu) -
zależy nam na audycie, tylko wolimy mieć czas na łatkę zanim szczegóły trafią do sieci.

## Licencja

Wysyłając PR zgadzasz się, że Twój wkład trafia pod tę samą licencję co reszta repo
(`LICENSE`).
