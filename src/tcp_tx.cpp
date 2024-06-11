#include <Arduino.h>

#include "queues.h"
#include "tcp_tx.h"
#include "tcp_ctrl.h"

void tcpTxTask(void *param)
{
    tcp_tx_msg_s msg;

    Serial.println("Create tcpTxTask");

    while (true)
    {
        if (xQueueReceive(tcpTxQueue, &msg, portMAX_DELAY) == pdPASS)
        {
            Serial.println("Send TCP data");
            tcp_ctrl_sendToAllClients(msg.data, msg.len);
        }
    }
}