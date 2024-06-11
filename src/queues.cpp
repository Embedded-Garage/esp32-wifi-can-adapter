#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "queues.h"

QueueHandle_t canRxQueue;
const int rxQueueSize = 10;

QueueHandle_t canControlQueue;
const int canControlQueueSize = 10;

QueueHandle_t tcpTxQueue;
const int tcpTxQueueSize = 10;

QueueHandle_t appQueue;
const int appQueueSize = 10;

