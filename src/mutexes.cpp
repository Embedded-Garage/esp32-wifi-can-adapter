#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "mutexes.h"

SemaphoreHandle_t xClientsMutex;

bool mutexes_init(void)
{
    xClientsMutex = xSemaphoreCreateMutex();

    return true;
}