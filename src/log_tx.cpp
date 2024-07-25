#include <Arduino.h>

#include "queues.h"
#include "tcp_tx.h"
#include "tcp_ctrl.h"

void logTxTask(void *param)
{
    tcp_tx_msg_s msg;

    Serial.println("AT+LOG_I=Create logTxTask");

    while (true)
    {
        if (xQueueReceive(logTxQueue, &msg, portMAX_DELAY) == pdPASS)
        {
            /* Send tcp msg */
            tcp_tx_msg_s tcp_tx_msg;
            memcpy(tcp_tx_msg.data, msg.data, msg.len);
            tcp_tx_msg.len = msg.len;
            xQueueSend(tcpTxQueue, &tcp_tx_msg, 0);

            /* Send serial msg */
            String serialMsg(msg.data, msg.len);
            Serial.print(serialMsg);
        }
    }
}