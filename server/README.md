# server

Magazyn wiadomości store-and-forward dla odbiorców chwilowo offline + katalog kluczy
publicznych (rozdz. 10 pełnego planu). Nie zaczęte - to krok 3 w kolejności prac,
po ustabilizowaniu warstwy radiowej/mesh i integracji szyfrowania w `/firmware`.

Planowany stos: Rust (Axum) lub Go, PostgreSQL, Redis do kolejki wiadomości oczekujących
na dostarczenie. Serwer przechowuje wyłącznie zaszyfrowane blob-y - nie ma technicznej
możliwości odczytania treści (patrz model zaufania w rozdz. 4.3).
