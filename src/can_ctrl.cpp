#include <Arduino.h>
#include "driver/twai.h"

#include "can_ctrl.h"
#include "queues.h"
#include "config.h"

void canControlTask(void *param)
{
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = {.acceptance_code = 0xFD000000, .acceptance_mask = 0x001FFFFF, .single_filter = true};

  Serial.println("AT+LOG_I=Create canControlTask");
  // Install TWAI driver
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK)
  {
    printf("AT+LOG_I=TWAI Driver installed\n");
  }
  else
  {
    printf("AT+LOG_E=Failed to install TWAI driver\n");
    return;
  }
  // Start TWAI driver
  if (twai_start() == ESP_OK)
  {
    printf("AT+LOG_I=TWAI Driver started\n");
  }
  else
  {
    printf("AT+LOG_E=Failed to start TWAI driver\n");
    return;
  }

  can_ctrl_msg_s msg;

  while (true)
  {
    if (xQueueReceive(canControlQueue, &msg, portMAX_DELAY) == pdPASS)
    {
      bool twai_reconfig = false;

      switch (msg.type)
      {
      case CAN_CTRL_MSG_TYPE_TX_MSG:
        twai_clear_transmit_queue();
        twai_transmit(&msg.msg, portMAX_DELAY);
        break;
      case CAN_CTRL_MSG_TYPE_SET_MODE:
        g_config.mode = msg.mode;
        twai_reconfig = true;
        break;
      case CAN_CTRL_MSG_TYPE_SET_SPEED:
        t_config = msg.speed;
        twai_reconfig = true;
        break;
      case CAN_CTRL_MSG_TYPE_SET_FILTER:
        f_config = msg.filter;
        twai_reconfig = true;
        break;
      }

      if (twai_reconfig)
      {
        twai_stop();
        twai_driver_uninstall();
        twai_driver_install(&g_config, &t_config, &f_config);
        twai_start();
      }
    }
  }
}
