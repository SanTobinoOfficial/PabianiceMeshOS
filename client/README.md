# client

Dwie różne drogi łączenia się z siecią Pabianice Comms, zależnie od tego czy masz
internet/ethernet czy nie:

- **`desktop/`** - prawdziwa aplikacja (Rust, CLI) na komputer z internetem/ethernetem.
  Łączy się z `/server` po HTTP, używa dokładnie tego samego kodu Signal Protocol co
  firmware (patrz `desktop/README.md` po wyjaśnienie jak). Przetestowana end-to-end
  na realnie odpalonym serwerze - publikacja kluczy, X3DH, wymiana wiadomości w obie
  strony.
- **`web-ble/`** - strona przeglądarkowa (Web Bluetooth) dla telefonu **bez internetu**.
  Łączy się bezpośrednio po BLE z najbliższym węzłem mesh (`firmware/components/ble`),
  ten węzeł robi za telefon całą kryptografię i routing przez LoRa. Bez Xcode/Androida,
  bez sklepu z aplikacjami - otwierasz stronę w Chrome i tyle (Safari/iOS nie wspiera
  Web Bluetooth, patrz `web-ble/README.md`).

Natywna aplikacja mobilna (Android/iOS z własnym UI, listą kontaktów, historią) to
osobna, dużo większa robota (rozdz. 9.4 planu) - `web-ble/` jest tego namiastką, którą
dało się faktycznie napisać i częściowo zweryfikować bez fizycznego telefonu/węzła.
