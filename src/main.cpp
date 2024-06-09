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

// Konfiguracja CAN
QueueHandle_t canQueue;
const int rx_queue_size = 10;
twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_27, GPIO_NUM_26, TWAI_MODE_NORMAL);
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

// Konfiguracja WiFi
const char *default_ssid = "miki3";
const char *default_password = "mikimiki";
char ssid[32];
char password[64];
Preferences preferences;

// Konfiguracja serwera TCP
AsyncServer *server = NULL;
std::vector<AsyncClient *> clients;

void onClientConnected(void *arg, AsyncClient *client);
void onClientDisconnected(void *arg, AsyncClient *client);
void onClientData(void *arg, AsyncClient *client, void *data, size_t len);

void canReceiveTask(void *param);
void clientSendTask(void *param);

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
  canQueue = xQueueCreate(rx_queue_size, sizeof(twai_message_t));

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

  // Utworzenie zadań FreeRTOS
  xTaskCreatePinnedToCore(canReceiveTask, "CAN Receive Task", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(clientSendTask, "Client Send Task", 4096, NULL, 1, NULL, 1);
}

void loop()
{
  // Main loop is empty as we are using FreeRTOS tasks
}

void canReceiveTask(void *param)
{
  twai_message_t message;

  while (true)
  {
    if (twai_receive(&message, pdMS_TO_TICKS(10)) == ESP_OK)
    {
      xQueueSend(canQueue, &message, portMAX_DELAY);
    }
  }
}

void clientSendTask(void *param)
{
  twai_message_t message;
  while (true)
  {
    if (xQueueReceive(canQueue, &message, portMAX_DELAY) == pdPASS)
    {
      char frameData[128];
      sprintf(frameData, "AT+RECV=%03X,%d,%d,", message.identifier, message.data_length_code, message.extd);
      for (int i = 0; i < message.data_length_code; i++)
      {
        sprintf(frameData + strlen(frameData), "%02X", message.data[i]);
      }
      strcat(frameData, "\r\n");

      Serial.println(frameData);

      // Wysłanie ramki CAN do wszystkich podłączonych klientów
      for (auto client : clients)
      {
        if (client->connected())
        {
          client->write(frameData);
        }
      }
    }
  }
}

void onClientConnected(void *arg, AsyncClient *client)
{
  Serial.println("New client connected");
  client->onData(&onClientData, client);
  client->onDisconnect(&onClientDisconnected, client);
  clients.push_back(client);
}

void onClientDisconnected(void *arg, AsyncClient *client)
{
  Serial.println("Client disconnected");
  clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
}

void onClientData(void *arg, AsyncClient *client, void *data, size_t len)
{
  String command = String((char *)data).substring(0, len);
  Serial.println("Received command: " + command);

  // Przetwarzanie komend AT
  if (command.startsWith("AT+SPEED="))
  {
    // Przykladowa implementacja zmiany prędkości
    int speed = command.substring(9).toInt();
    switch (speed)
    {
    case 1000:
      t_config = TWAI_TIMING_CONFIG_1MBITS();
      break;
    case 800:
      t_config = TWAI_TIMING_CONFIG_800KBITS();
      break;
    case 500:
      t_config = TWAI_TIMING_CONFIG_500KBITS();
      break;
    case 250:
      t_config = TWAI_TIMING_CONFIG_250KBITS();
      break;
    case 125:
      t_config = TWAI_TIMING_CONFIG_125KBITS();
      break;
    case 100:
      t_config = TWAI_TIMING_CONFIG_100KBITS();
      break;
    case 50:
      t_config = TWAI_TIMING_CONFIG_50KBITS();
      break;
    default:
      t_config = TWAI_TIMING_CONFIG_1MBITS();
      break;
    }
    twai_stop();
    twai_driver_uninstall();
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK)
    {
      client->write("OK\r\n");
    }
    else
    {
      client->write("ERROR\r\n");
    }
  }
  else if (command.startsWith("AT+FILTER="))
  {
    int idx1 = command.indexOf(',');
    int idx2 = command.indexOf(',', idx1 + 1);

    f_config.acceptance_code = (uint32_t)strtol(command.substring(10, idx1).c_str(), NULL, 16);
    f_config.acceptance_mask = (uint32_t)strtol(command.substring(idx1 + 1, idx2).c_str(), NULL, 16);
    f_config.single_filter = command.substring(idx2 + 1).toInt();

    twai_stop();
    twai_driver_uninstall();
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK)
    {
      client->write("OK\r\n");
    }
    else
    {
      client->write("ERROR\r\n");
    }
  }
  else if (command.startsWith("AT+SEND="))
  {
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

    twai_message_t message;
    message.extd = extd;
    message.identifier = msgID;
    message.data_length_code = dlc;

    for (int i = 0; i < dlc; i++)
    {
      String byteStr = dataStr.substring(i * 2, i * 2 + 2);
      message.data[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
    }
    twai_clear_transmit_queue();
    esp_err_t err = twai_transmit(&message, pdMS_TO_TICKS(100));
    if (err == ESP_OK)
    {
      client->write("OK\r\n");
    }
    else
    {
      char response[128];
      sprintf(response, "ERROR: %d\r\n", err);
      client->write(response);
    }
  }
  else if (command.startsWith("AT+WIFI_SSID="))
  {
    String new_ssid = command.substring(13);
    new_ssid.toCharArray(ssid, sizeof(ssid));
    preferences.putString("ssid", ssid);
    client->write("OK\r\n");
  }
  else if (command.startsWith("AT+WIFI_PASS="))
  {
    String new_password = command.substring(13);
    new_password.toCharArray(password, sizeof(password));
    preferences.putString("password", password);
    client->write("OK\r\n");
  }
  else
  {
    client->write("ERROR\r\n");
  }
}
