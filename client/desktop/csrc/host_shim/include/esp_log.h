#pragma once

// zamiennik ESP_LOGI/ESP_LOGW - na hoscie po prostu na stderr, zeby nie mieszac sie
// z ewentualnym stdout output CLI

#include <stdio.h>

#define ESP_LOGI(tag, fmt, ...) fprintf(stderr, "I (%s): " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "W (%s): " fmt "\n", tag, ##__VA_ARGS__)
