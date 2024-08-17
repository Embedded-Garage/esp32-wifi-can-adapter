#pragma once

#include "driver/twai.h"
#include "tcp_tx.h"

typedef tcp_tx_msg_s tcp_rx_msg_s;
typedef tcp_tx_msg_s serial_rx_msg_s;

typedef enum
{
    APP_MSG_TYPE_CAN_RX,
    APP_MSG_TYPE_TCP_RX,
    APP_MSG_TYPE_SERIAL_RX,
} app_msg_type_e;

typedef struct
{
    app_msg_type_e type;
    union
    {
        struct
        {
            uint64_t timestamp;
            uint8_t direction;
            twai_message_t can_msg;
        };
        tcp_rx_msg_s tcp_rx_msg;
        serial_rx_msg_s serial_rx_msg;
    };
} app_msg_s;

extern void appTask(void *param);