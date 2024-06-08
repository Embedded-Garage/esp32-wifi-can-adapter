#include <Arduino.h>
#include "config.h"
#include <HardwareSerial.h>
#include <ESP32CAN.h>
#include <CAN_config.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <vector>

// Konfiguracja CAN
CAN_device_t CAN_cfg;
unsigned long previousMillis = 0;
const int interval = 1000;
const int rx_queue_size = 10;

// Konfiguracja WiFi
const char* ssid = "miki3";
const char* password = "mikimiki";

// Konfiguracja serwera TCP
AsyncServer *server = NULL;
std::vector<AsyncClient*> clients;

void onClientConnected(void* arg, AsyncClient* client);
void onClientDisconnected(void* arg, AsyncClient* client);
void onClientData(void* arg, AsyncClient* client, void *data, size_t len);

void setup() {
  pinMode(PIN_5V_EN, OUTPUT);
  digitalWrite(PIN_5V_EN, HIGH);

  pinMode(CAN_SE_PIN, OUTPUT);
  digitalWrite(CAN_SE_PIN, LOW);

  Serial.begin(115200);

  Serial.println("Basic Demo - ESP32-Arduino-CAN");
  CAN_cfg.speed = CAN_SPEED_1000KBPS;
  CAN_cfg.tx_pin_id = GPIO_NUM_27;
  CAN_cfg.rx_pin_id = GPIO_NUM_26;
  CAN_cfg.rx_queue = xQueueCreate(rx_queue_size, sizeof(CAN_frame_t));
  
  // Inicjalizacja modułu CAN
  ESP32Can.CANInit();

  Serial.print("CAN SPEED :");
  Serial.println(CAN_cfg.speed);

  // Połączenie z WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Utworzenie serwera TCP
  server = new AsyncServer(1234); // port 1234
  server->onClient(&onClientConnected, server);
  server->begin();
}

void loop() {
  CAN_frame_t rx_frame;
  unsigned long currentMillis = millis();

  // Odbieranie ramek CAN
  if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, pdMS_TO_TICKS(10)) == pdTRUE) {
    char frameData[128];
    sprintf(frameData, "AT+RECV=%03X,%d,", rx_frame.MsgID, rx_frame.FIR.B.DLC);
    for (int i = 0; i < rx_frame.FIR.B.DLC; i++) {
      sprintf(frameData + strlen(frameData), "%02X", rx_frame.data.u8[i]);
    }
    strcat(frameData, "\r\n");

    Serial.println(frameData);

    // Wysłanie ramki CAN do wszystkich podłączonych klientów
    for (auto client : clients) {
      if (client->connected()) {
        client->write(frameData);
      }
    }
  }
}

void onClientConnected(void* arg, AsyncClient* client) {
  Serial.println("New client connected");
  client->onData(&onClientData, client);
  client->onDisconnect(&onClientDisconnected, client);
  clients.push_back(client);
}

void onClientDisconnected(void* arg, AsyncClient* client) {
  Serial.println("Client disconnected");
  clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
}

void onClientData(void* arg, AsyncClient* client, void *data, size_t len) {
  String command = String((char*)data).substring(0, len);
  Serial.println("Received command: " + command);

  // Przetwarzanie komend AT
  if (command.startsWith("AT+SPEED=")) {
    int speed = command.substring(9).toInt();
    CAN_cfg.speed = (CAN_speed_t)speed;
    ESP32Can.CANInit();
    client->write("OK\r\n");
  } else if (command.startsWith("AT+FILTER=")) {
    int idx1 = command.indexOf(',');
    int idx2 = command.indexOf(',', idx1 + 1);
    int idx3 = command.indexOf(',', idx2 + 1);
    int idx4 = command.indexOf(',', idx3 + 1);
    int idx5 = command.indexOf(',', idx4 + 1);
    int idx6 = command.indexOf(',', idx5 + 1);
    int idx7 = command.indexOf(',', idx6 + 1);
    int idx8 = command.indexOf(',', idx7 + 1);

    CAN_filter_t filter;
    filter.FM = (CAN_filter_mode_t)command.substring(10, idx1).toInt();
    filter.ACR0 = (uint8_t)strtol(command.substring(idx1 + 1, idx2).c_str(), NULL, 16);
    filter.ACR1 = (uint8_t)strtol(command.substring(idx2 + 1, idx3).c_str(), NULL, 16);
    filter.ACR2 = (uint8_t)strtol(command.substring(idx3 + 1, idx4).c_str(), NULL, 16);
    filter.ACR3 = (uint8_t)strtol(command.substring(idx4 + 1, idx5).c_str(), NULL, 16);
    filter.AMR0 = (uint8_t)strtol(command.substring(idx5 + 1, idx6).c_str(), NULL, 16);
    filter.AMR1 = (uint8_t)strtol(command.substring(idx6 + 1, idx7).c_str(), NULL, 16);
    filter.AMR2 = (uint8_t)strtol(command.substring(idx7 + 1, idx8).c_str(), NULL, 16);
    filter.AMR3 = (uint8_t)strtol(command.substring(idx8 + 1).c_str(), NULL, 16);

    ESP32Can.CANConfigFilter(&filter);
    ESP32Can.CANStop();
    ESP32Can.CANInit();
    client->write("OK\r\n");
  } else if (command.startsWith("AT+SEND=")) {
    int idx1 = command.indexOf(',');
    int idx2 = command.indexOf(',', idx1 + 1);
    String msgIDStr = command.substring(8, idx1);
    String dlcStr = command.substring(idx1 + 1, idx2);
    String dataStr = command.substring(idx2 + 1);

    int msgID = (int) strtol(msgIDStr.c_str(), NULL, 16);
    int dlc = dlcStr.toInt();
    
    CAN_frame_t tx_frame;
    tx_frame.FIR.B.FF = CAN_frame_std;
    tx_frame.MsgID = msgID;
    tx_frame.FIR.B.DLC = dlc;
    
    for (int i = 0; i < dlc; i++) {
      String byteStr = dataStr.substring(i * 2, i * 2 + 2);
      tx_frame.data.u8[i] = (uint8_t) strtol(byteStr.c_str(), NULL, 16);
    }

    ESP32Can.CANWriteFrame(&tx_frame);
    client->write("OK\r\n");
  } else {
    client->write("ERROR\r\n");
  }
}
