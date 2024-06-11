#pragma once

#include "driver/twai.h"

typedef enum
{
  CAN_CTRL_MSG_TYPE_TX_MSG,
  CAN_CTRL_MSG_TYPE_SET_MODE,
  CAN_CTRL_MSG_TYPE_SET_SPEED,
  CAN_CTRL_MSG_TYPE_SET_FILTER,
} can_ctrl_msg_type_e;

typedef struct
{
  can_ctrl_msg_type_e type;
  union
  {
    twai_message_t msg;
    twai_mode_t mode;
    twai_timing_config_t speed;
    twai_filter_config_t filter;
  };
} can_ctrl_msg_s;

void canControlTask(void *param);