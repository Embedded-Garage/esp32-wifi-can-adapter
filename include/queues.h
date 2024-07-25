#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

extern QueueHandle_t canRxQueue;
extern const int rxQueueSize;

extern QueueHandle_t canControlQueue;
extern const int canControlQueueSize;

extern QueueHandle_t tcpTxQueue;
extern const int tcpTxQueueSize;

extern QueueHandle_t appQueue;
extern const int appQueueSize;

extern QueueHandle_t serialTxQueue;
extern const int serialTxQueueSize;

extern QueueHandle_t logTxQueue;
extern const int logTxQueueSize;

extern bool queues_init(void);
