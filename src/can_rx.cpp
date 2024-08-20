#include <Arduino.h>

#include "driver/twai.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "queues.h"
#include "app.h"
#include "statistics.h"

void canRxTask(void *param)
{
    app_msg_s app_message = {.type = APP_MSG_TYPE_CAN_RX};
    Serial.println("AT+LOG_I=Create canRxTask");

    while (true)
    {
        if (twai_receive(&app_message.can_msg, 1) == ESP_OK)
        {
            statistics.total_rcvd_can_frames++;
            app_message.timestamp = (uint64_t)esp_timer_get_time();
            app_message.direction = CAN_MSG_DIR_RX;
            if (pdPASS != xQueueSend(appQueue, &app_message, 0))
            {
                statistics.lost_frames_can_to_app++;
            }
        }
    }
}