#include <Arduino.h>
#include "config.h"
#include <HardwareSerial.h>
#include <ESP32CAN.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <vector>
#include <Preferences.h>
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

typedef struct
{
  char data[256];
  int len;
} tcp_tx_msg_s;

typedef tcp_tx_msg_s txp_rx_msg_s;

typedef enum
{
  APP_MSG_TYPE_CAN_RX,
  APP_MSG_TYPE_TCP_RX,
} app_msg_type_e;

typedef struct
{
  app_msg_type_e type;
  union
  {
    twai_message_t can_msg;
    txp_rx_msg_s tcp_rx_msg;
  };
} app_msg_s;

// Konfiguracja CAN
QueueHandle_t canRxQueue;
const int rx_queue_size = 10;

QueueHandle_t canControlQueue;
const int control_queue_size = 10;

QueueHandle_t tcpTxQueue;
const int tcp_tx_queue_size = 10;

QueueHandle_t appQueue;
const int app_queue_size = 10;

// Konfiguracja WiFi
const char *default_ssid = "miki3";
const char *default_password = "mikimiki";
char ssid[32];
char password[64];
Preferences preferences;

// Konfiguracja serwera TCP
AsyncServer *server = NULL;
SemaphoreHandle_t xClientsMutex;
std::vector<AsyncClient *> clients;

void onClientConnected(void *arg, AsyncClient *client);
void onClientDisconnected(void *arg, AsyncClient *client);
void onClientData(void *arg, AsyncClient *client, void *data, size_t len);

void canRxTask(void *param);
void canControlTask(void *param);
void tcpTxTask(void *param);
void appTask(void *param);

void setup()
{
  pinMode(PIN_5V_EN, OUTPUT);
  digitalWrite(PIN_5V_EN, HIGH);

  pinMode(CAN_SE_PIN, OUTPUT);
  digitalWrite(CAN_SE_PIN, LOW);

  Serial.begin(115200);

  Serial.println("Basic Demo - ESP32-Arduino-CAN");

  preferences.begin("wifi", false);
  if (preferences.isKey("ssid"))
  {
    preferences.getString("ssid", ssid, sizeof(ssid));
  }
  else
  {
    strcpy(ssid, default_ssid);
  }

  if (preferences.isKey("password"))
  {
    preferences.getString("password", password, sizeof(password));
  }
  else
  {
    strcpy(password, default_password);
  }

  // Połączenie z WiFi
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Failed to connect to WiFi. Starting Access Point.");
    WiFi.softAP("EmbeddedGarage_CAN_tool");
    Serial.print("Access Point IP: ");
    Serial.println(WiFi.softAPIP());
  }
  else
  {
    Serial.println("Connected to WiFi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  // Utworzenie serwera TCP
  server = new AsyncServer(1234); // port 1234
  server->onClient(&onClientConnected, server);
  server->begin();

  // Utworzenie kolejki dla ramek CAN
  canRxQueue = xQueueCreate(rx_queue_size, sizeof(twai_message_t));
  canControlQueue = xQueueCreate(control_queue_size, sizeof(can_ctrl_msg_s));
  tcpTxQueue = xQueueCreate(tcp_tx_queue_size, sizeof(tcp_tx_msg_s));
  appQueue = xQueueCreate(app_queue_size, sizeof(app_msg_s));

  xClientsMutex = xSemaphoreCreateMutex();

  // Utworzenie zadań FreeRTOS
  xTaskCreatePinnedToCore(canRxTask, "CAN Rx Task", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(canControlTask, "CAN Control Task", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(appTask, "App Task", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(tcpTxTask, "TCP Tx Task", 4096, NULL, 1, NULL, 0);
}

void loop()
{
  // Main loop is empty as we are using FreeRTOS tasks
}

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

void canControlTask(void *param)
{
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_27, GPIO_NUM_26, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = {.acceptance_code = 0xFD000000, .acceptance_mask = 0x001FFFFF, .single_filter = true};

  Serial.println("Create canControlTask");
  // Install TWAI driver
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK)
  {
    printf("Driver installed\n");
  }
  else
  {
    printf("Failed to install driver\n");
    return;
  }
  // Start TWAI driver
  if (twai_start() == ESP_OK)
  {
    printf("Driver started\n");
  }
  else
  {
    printf("Failed to start driver\n");
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
        Serial.println("CAN_CTRL_MSG_TYPE_TX_MSG");
        break;
      case CAN_CTRL_MSG_TYPE_SET_MODE:
        g_config.mode = msg.mode;
        twai_reconfig = true;
        Serial.println("CAN_CTRL_MSG_TYPE_SET_MODE");
        break;
      case CAN_CTRL_MSG_TYPE_SET_SPEED:
        t_config = msg.speed;
        twai_reconfig = true;
        Serial.println("CAN_CTRL_MSG_TYPE_SET_SPEED");
        break;
      case CAN_CTRL_MSG_TYPE_SET_FILTER:
        f_config = msg.filter;
        twai_reconfig = true;
        Serial.println("CAN_CTRL_MSG_TYPE_SET_FILTER");
        break;
      }

      if (twai_reconfig)
      {
        Serial.println("TWAI reconfig");
        twai_stop();
        twai_driver_uninstall();
        twai_driver_install(&g_config, &t_config, &f_config);
        twai_start();
      }
    }
  }
}

void tcpTxTask(void *param)
{
  tcp_tx_msg_s msg;

  Serial.println("Create tcpTxTask");

  while (true)
  {
    if (xQueueReceive(tcpTxQueue, &msg, portMAX_DELAY) == pdPASS)
    {
      Serial.println("Send TCP data");
      xSemaphoreTake(xClientsMutex, portMAX_DELAY);
      for (auto client : clients)
      {
        if (client->connected())
        {
          client->write(msg.data, msg.len);
        }
      }
      xSemaphoreGive(xClientsMutex);
    }
  }
}

void appTask(void *param)
{
  app_msg_s msg;
  tcp_tx_msg_s tcp_tx_msg;
  can_ctrl_msg_s can_ctrl_msg;

  Serial.println("Create tcpTxTask");

  while (true)
  {
    if (xQueueReceive(appQueue, &msg, portMAX_DELAY) == pdPASS)
    {
      switch (msg.type)
      {
      case APP_MSG_TYPE_CAN_RX:
        Serial.println("APP_MSG_TYPE_CAN_RX");
        tcp_tx_msg.len = sprintf(tcp_tx_msg.data,
                                 "AT+RECV=%03X,%d,%d,",
                                 msg.can_msg.identifier,
                                 msg.can_msg.data_length_code,
                                 msg.can_msg.extd);
        for (int i = 0; i < msg.can_msg.data_length_code; i++)
        {
          tcp_tx_msg.len += sprintf(&tcp_tx_msg.data[tcp_tx_msg.len], "%02X", msg.can_msg.data[i]);
        }
        tcp_tx_msg.len += sprintf(&tcp_tx_msg.data[tcp_tx_msg.len], "\r\n");

        xQueueSend(tcpTxQueue, &tcp_tx_msg, 0);
        break;
      case APP_MSG_TYPE_TCP_RX:
      {
        Serial.println("APP_MSG_TYPE_TCP_RX");
        String command = String(msg.tcp_rx_msg.data).substring(0, msg.tcp_rx_msg.len);
        Serial.println("Received command: " + command);
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
            tcp_tx_msg.len = sprintf(tcp_tx_msg.data, "ERROR\r\n");
            xQueueSend(tcpTxQueue, &tcp_tx_msg, 0);
          }
          else
          {
            xQueueSend(canControlQueue, &can_ctrl_msg, 0);
            tcp_tx_msg.len = sprintf(tcp_tx_msg.data, "OK\r\n");
            xQueueSend(tcpTxQueue, &tcp_tx_msg, 0);
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

          Serial.printf("Set filter to: 0x%08x 0x%08x %d",
                        can_ctrl_msg.filter.acceptance_code,
                        can_ctrl_msg.filter.acceptance_mask,
                        can_ctrl_msg.filter.single_filter);
          xQueueSend(canControlQueue, &can_ctrl_msg, 0);
          tcp_tx_msg.len = sprintf(tcp_tx_msg.data, "OK\r\n");
          xQueueSend(tcpTxQueue, &tcp_tx_msg, 0);
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
          tcp_tx_msg.len = sprintf(tcp_tx_msg.data, "OK\r\n");
          xQueueSend(tcpTxQueue, &tcp_tx_msg, 0);
        }
        else
        {
          tcp_tx_msg.len = sprintf((char *)tcp_tx_msg.data, "ERROR\r\n");
          xQueueSend(tcpTxQueue, &tcp_tx_msg, 0);
        }
      }
      break;
      }
    }
  }
}

void onClientConnected(void *arg, AsyncClient *client)
{
  Serial.println("New client connected");
  client->onData(&onClientData, client);
  client->onDisconnect(&onClientDisconnected, client);

  xSemaphoreTake(xClientsMutex, portMAX_DELAY);
  clients.push_back(client);
  xSemaphoreGive(xClientsMutex);
}

void onClientDisconnected(void *arg, AsyncClient *client)
{
  Serial.println("Client disconnected");
  xSemaphoreTake(xClientsMutex, portMAX_DELAY);
  clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
  xSemaphoreGive(xClientsMutex);
}

void onClientData(void *arg, AsyncClient *client, void *data, size_t len)
{
  app_msg_s app_msg = {.type = APP_MSG_TYPE_TCP_RX};
  if (len <= sizeof(app_msg.tcp_rx_msg.data))
  {
    memcpy(app_msg.tcp_rx_msg.data, data, len);
    app_msg.tcp_rx_msg.len = len;
    xQueueSend(appQueue, &app_msg, 0);
  }
}
