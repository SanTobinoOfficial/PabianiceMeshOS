#pragma once

// pcrypto.c uzywa rekurencyjnego mutexu FreeRTOS tylko po to, zeby jeden watek na raz
// wchodzil w signal_context - na hoscie to zwykly pthread rekurencyjny mutex (shim.c)

#include <stdint.h>

typedef struct semphr_shim *SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void);
int xSemaphoreTakeRecursive(SemaphoreHandle_t sem, uint32_t timeout);
int xSemaphoreGiveRecursive(SemaphoreHandle_t sem);
