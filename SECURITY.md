# Bezpieczeństwo

To projekt krytyczny dla prywatności komunikacji (szyfrowanie end-to-end, Signal
Protocol) - luki bezpieczeństwa traktujemy poważnie, mimo że to wciąż wczesny etap
hobbystyczny (Faza 1/2 planu).

## Zgłaszanie podatności

**Nie zgłaszaj realnych luk bezpieczeństwa jako zwykłego, publicznego issue.** Zamiast
tego skorzystaj z prywatnego zgłoszenia przez zakładkę **Security → Report a
vulnerability** w tym repozytorium (GitHub Security Advisories) - trafi tylko do osoby
prowadzącej projekt, nie na widok publiczny, zanim będzie łatka.

Dotyczy w szczególności:

- błędów w implementacji warstwy kryptograficznej (`firmware/components/crypto`,
  integracja z libsignal-protocol-c) mogących osłabić poufność lub autentyczność wiadomości,
- błędów w warstwie mesh/routingu (`firmware/components/mesh`) pozwalających na spoofing,
  DoS wykraczający poza znane ograniczenia rate-limitingu, albo obejście dedup/TTL w sposób
  szkodliwy dla sieci,
- luk w `/server` pozwalających na nieautoryzowany dostęp do magazynu wiadomości, katalogu
  kluczy albo listy blokad,
- podatności w zależnościach (libsignal-protocol-c, crates Rust, komponenty ESP-IDF).

## Co NIE jest podatnością (znane, udokumentowane ograniczenia)

- Widoczność metadanych routingu (kto z kim, kiedy, ile) dla kogoś podsłuchującego
  transmisję radiową - fundamentalne ograniczenie architektury, opisane w rozdz. 6.5 planu.
- Brak trwałej bariery przed zbudowaniem nowego urządzenia z nowym HWID po zablokowaniu
  starego - świadomy kompromis modelu open source + DIY, rozdz. 7.3.3 planu.
- Klucze w NVS zamiast w module bezpiecznym (ATECC608A) na obecnym etapie sprzętowym -
  już udokumentowany dług techniczny, patrz `firmware/README.md`.

Zgłoszenia dotyczące powyższego możesz śmiało wrzucać jako zwykłe issue/dyskusję -
to nie sekrety, tylko rzeczy do rozwiązania w kolejnych fazach.

## Czas odpowiedzi

Projekt jest prowadzony hobbystycznie w wolnym czasie - staramy się potwierdzić
otrzymanie zgłoszenia w ciągu kilku dni, ale to nie firma z SLA. Krytyczne, łatwe do
wykorzystania luki w warstwie kryptograficznej mają priorytet.
