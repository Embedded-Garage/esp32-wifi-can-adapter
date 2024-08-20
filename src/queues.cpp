#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "driver/twai.h"
#include "can_ctrl.h"
#include "can_rx.h"
#include "tcp_tx.h"
#include "log_tx.h"
#include "app.h"

#include "queues.h"

QueueHandle_t canRxQueue;
const int rxQueueSize = 10;

QueueHandle_t canControlQueue;
const int canControlQueueSize = 10;

QueueHandle_t tcpTxQueue;
const int tcpTxQueueSize = 10;

QueueHandle_t appQueue;
const int appQueueSize = 50;

QueueHandle_t serialTxQueue;
const int serialTxQueueSize = 10;

QueueHandle_t logTxQueue;
const int logTxQueueSize = 100;

bool queues_init(void)
{
    canRxQueue = xQueueCreate(rxQueueSize, sizeof(twai_message_t));
    canControlQueue = xQueueCreate(canControlQueueSize, sizeof(can_ctrl_msg_s));
    tcpTxQueue = xQueueCreate(tcpTxQueueSize, sizeof(tcp_tx_msg_s));
    appQueue = xQueueCreate(appQueueSize, sizeof(app_msg_s));
    logTxQueue = xQueueCreate(logTxQueueSize, sizeof(log_tx_msg_s));

    return true;
}