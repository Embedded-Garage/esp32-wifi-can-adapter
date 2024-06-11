#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "mutexes.h"

SemaphoreHandle_t xClientsMutex;