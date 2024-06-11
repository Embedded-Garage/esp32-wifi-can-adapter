#include <Arduino.h>

#include "driver/twai.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "queues.h"
#include "app.h"

void canRxTask(void *param)
{
    app_msg_s app_message = {.type = APP_MSG_TYPE_CAN_RX};
    Serial.println("Create canRxTask");

    while (true)
    {
        if (twai_receive(&app_message.can_msg, 0) == ESP_OK)
        {
            xQueueSend(appQueue, &app_message, 0);
            Serial.println("Sent CAN RX message to app");
        }
        else
        {
            vTaskDelay(1);
        }
    }
}