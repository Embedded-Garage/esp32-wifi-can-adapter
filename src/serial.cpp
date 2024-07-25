#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "queues.h"
#include "app.h"

void serialRxTask(void *parameter)
{
    app_msg_s msg = {.type = APP_MSG_TYPE_SERIAL_RX, .serial_rx_msg = {.len = 0}};
    bool valid = true;

    while (true)
    {
        while (Serial.available() > 0)
        {
            char ch = Serial.read();
            Serial.print(ch);

            if (ch == '\r')
            {
                continue;
            }
            else if (ch == '\n')
            {
                if (valid)
                {
                    xQueueSend(appQueue, &msg, 0);
                }
                else
                {
                    valid = true;
                }
                msg.serial_rx_msg.len = 0u;
            }
            else
            {
                if (msg.serial_rx_msg.len < (sizeof(msg.serial_rx_msg.data) - 1))
                {
                    msg.serial_rx_msg.data[msg.serial_rx_msg.len++] = ch;
                    msg.serial_rx_msg.data[msg.serial_rx_msg.len] = '\0';
                }
                else
                {
                    valid = false;
                }
            }
        }
        vTaskDelay(1);
    }
}