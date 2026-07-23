#pragma once

// Nie ma odpowiednika w ESP-IDF - to jedyna funkcja shimu wolana bezposrednio z Rusta
// (przed pcrypto_init()), zeby powiedziec plikowemu "NVS" gdzie ma trzymac dane danej
// tozsamosci. Bez tego wszystkie procesy dzieliłyby jeden katalog domyslny.

void host_shim_set_data_dir(const char *dir);
