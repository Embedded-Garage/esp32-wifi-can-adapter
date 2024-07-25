#include <Arduino.h>
#include <Preferences.h>

#include "can_ctrl.h"
#include "log_tx.h"

#include "queues.h"
#include "app.h"

void appTask(void *param)
{
    app_msg_s msg;
    log_tx_msg_s log_tx_msg;
    can_ctrl_msg_s can_ctrl_msg;
    Preferences preferences;

    preferences.begin("wifi", false);

    Serial.println("AT+LOG_I=Create tcpTxTask");

    while (true)
    {
        if (xQueueReceive(appQueue, &msg, portMAX_DELAY) == pdPASS)
        {
            switch (msg.type)
            {
            case APP_MSG_TYPE_CAN_RX:
                log_tx_msg.len = sprintf(log_tx_msg.data,
                                         "AT+RECV=%03X,%d,%d,",
                                         msg.can_msg.identifier,
                                         msg.can_msg.data_length_code,
                                         msg.can_msg.extd);
                for (int i = 0; i < msg.can_msg.data_length_code; i++)
                {
                    log_tx_msg.len += sprintf(&log_tx_msg.data[log_tx_msg.len], "%02X", msg.can_msg.data[i]);
                }
                log_tx_msg.len += sprintf(&log_tx_msg.data[log_tx_msg.len], "\r\n");

                xQueueSend(logTxQueue, &log_tx_msg, 0);
                break;
            case APP_MSG_TYPE_TCP_RX:
            case APP_MSG_TYPE_SERIAL_RX:
            {
                String command = String(msg.tcp_rx_msg.data).substring(0, msg.tcp_rx_msg.len);
                // Przetwarzanie komend AT
                if (command.startsWith("AT+SPEED="))
                {
                    bool fail = false;
                    can_ctrl_msg.type = CAN_CTRL_MSG_TYPE_SET_SPEED;
                    // Przykladowa implementacja zmiany prędkości
                    int speed = command.substring(9).toInt();
                    switch (speed)
                    {
                    case 1000:
                        can_ctrl_msg.speed = TWAI_TIMING_CONFIG_1MBITS();
                        break;
                    case 800:
                        can_ctrl_msg.speed = TWAI_TIMING_CONFIG_800KBITS();
                        break;
                    case 500:
                        can_ctrl_msg.speed = TWAI_TIMING_CONFIG_500KBITS();
                        break;
                    case 250:
                        can_ctrl_msg.speed = TWAI_TIMING_CONFIG_250KBITS();
                        break;
                    case 125:
                        can_ctrl_msg.speed = TWAI_TIMING_CONFIG_125KBITS();
                        break;
                    case 100:
                        can_ctrl_msg.speed = TWAI_TIMING_CONFIG_100KBITS();
                        break;
                    case 50:
                        can_ctrl_msg.speed = TWAI_TIMING_CONFIG_50KBITS();
                        break;
                    default:
                        fail = true;
                        break;
                    }

                    if (fail)
                    {
                        log_tx_msg.len = sprintf(log_tx_msg.data, "ERROR\r\n");
                        xQueueSend(logTxQueue, &log_tx_msg, 0);
                    }
                    else
                    {
                        xQueueSend(canControlQueue, &can_ctrl_msg, 0);
                        memcpy(log_tx_msg.data, msg.tcp_rx_msg.data, msg.tcp_rx_msg.len);
                        log_tx_msg.len = msg.tcp_rx_msg.len;
                        xQueueSend(logTxQueue, &log_tx_msg, 0);
                    }
                }
                else if (command.startsWith("AT+FILTER="))
                {
                    int idx1 = command.indexOf(',');
                    int idx2 = command.indexOf(',', idx1 + 1);

                    can_ctrl_msg.type = CAN_CTRL_MSG_TYPE_SET_FILTER;

                    can_ctrl_msg.filter.acceptance_code = (uint32_t)strtoul(command.substring(10, idx1).c_str(), NULL, 16);
                    can_ctrl_msg.filter.acceptance_mask = (uint32_t)strtoul(command.substring(idx1 + 1, idx2).c_str(), NULL, 16);
                    can_ctrl_msg.filter.single_filter = command.substring(idx2 + 1).toInt();

                    // Serial.printf("Set filter to: 0x%08x 0x%08x %d",
                    //               can_ctrl_msg.filter.acceptance_code,
                    //               can_ctrl_msg.filter.acceptance_mask,
                    //               can_ctrl_msg.filter.single_filter);
                    xQueueSend(canControlQueue, &can_ctrl_msg, 0);
                    memcpy(log_tx_msg.data, msg.tcp_rx_msg.data, msg.tcp_rx_msg.len);
                    log_tx_msg.len = msg.tcp_rx_msg.len;
                    xQueueSend(logTxQueue, &log_tx_msg, 0);
                }
                else if (command.startsWith("AT+SEND="))
                {
                    can_ctrl_msg.type = CAN_CTRL_MSG_TYPE_TX_MSG;

                    int idx1 = command.indexOf(',');
                    int idx2 = command.indexOf(',', idx1 + 1);
                    int idx3 = command.indexOf(',', idx2 + 1);
                    String msgIDStr = command.substring(8, idx1);
                    String dlcStr = command.substring(idx1 + 1, idx2);
                    String extdStr = command.substring(idx2 + 1, idx3);
                    String dataStr = command.substring(idx3 + 1);

                    int msgID = (int)strtol(msgIDStr.c_str(), NULL, 16);
                    int dlc = dlcStr.toInt();
                    int extd = extdStr.toInt();

                    can_ctrl_msg.msg.extd = extd;
                    can_ctrl_msg.msg.identifier = msgID;
                    can_ctrl_msg.msg.data_length_code = dlc;

                    for (int i = 0; i < dlc; i++)
                    {
                        String byteStr = dataStr.substring(i * 2, i * 2 + 2);
                        can_ctrl_msg.msg.data[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
                    }
                    xQueueSend(canControlQueue, &can_ctrl_msg, 0);
                    memcpy(log_tx_msg.data, msg.tcp_rx_msg.data, msg.tcp_rx_msg.len);
                    log_tx_msg.len = msg.tcp_rx_msg.len;
                    xQueueSend(logTxQueue, &log_tx_msg, 0);
                }
                else if (command.startsWith("AT+WIFI_SSID="))
                {
                    preferences.putString("ssid", command.substring(13));
                    memcpy(log_tx_msg.data, msg.tcp_rx_msg.data, msg.tcp_rx_msg.len);
                    log_tx_msg.len = msg.tcp_rx_msg.len;
                    xQueueSend(logTxQueue, &log_tx_msg, 0);
                }
                else if (command.startsWith("AT+WIFI_PASS="))
                {
                    preferences.putString("pass", command.substring(13));
                    memcpy(log_tx_msg.data, msg.tcp_rx_msg.data, msg.tcp_rx_msg.len);
                    log_tx_msg.len = msg.tcp_rx_msg.len;
                    xQueueSend(logTxQueue, &log_tx_msg, 0);
                }
                else
                {
                    // log_tx_msg.len = sprintf((char *)log_tx_msg.data, "ERROR\r\n");
                    // xQueueSend(logTxQueue, &log_tx_msg, 0);
                }
            }
            break;
            }
        }
    }
}