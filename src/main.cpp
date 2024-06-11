#include <Arduino.h>
#include "config.h"
#include <HardwareSerial.h>
#include <ESP32CAN.h>
#include <SPI.h>
#include <SD.h>
#include "can_ctrl.h"
#include "tcp_tx.h"
#include "app.h"
#include "queues.h"
#include "mutexes.h"
#include "wifi_ctrl.h"
#include "tcp_ctrl.h"
#include "can_rx.h"

void setup()
{
  pinMode(PIN_5V_EN, OUTPUT);
  digitalWrite(PIN_5V_EN, HIGH);

  pinMode(CAN_SE_PIN, OUTPUT);
  digitalWrite(CAN_SE_PIN, LOW);

  Serial.begin(115200);

  Serial.println("Basic Demo - ESP32-Arduino-CAN");

  if (!wifi_ctrl_init())
  {
    Serial.println("Wifi initialization error!");
    return;
  }

  if (!tcp_ctrl_init())
  {
    Serial.println("Wifi initialization error!");
    return;
  }

  // Utworzenie kolejki dla ramek CAN
  canRxQueue = xQueueCreate(rxQueueSize, sizeof(twai_message_t));
  canControlQueue = xQueueCreate(canControlQueueSize, sizeof(can_ctrl_msg_s));
  tcpTxQueue = xQueueCreate(tcpTxQueueSize, sizeof(tcp_tx_msg_s));
  appQueue = xQueueCreate(appQueueSize, sizeof(app_msg_s));

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
