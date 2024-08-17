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
#include "log_tx.h"
#include "serial.h"

void setup()
{
  pinMode(PIN_5V_EN, OUTPUT);
  digitalWrite(PIN_5V_EN, HIGH);

  pinMode(CAN_SE_PIN, OUTPUT);
  digitalWrite(CAN_SE_PIN, LOW);

  Serial.begin(921600);

  Serial.println("AT+LOG_I=Embedded Garage - CAN/Wifi controller");

  if (!wifi_ctrl_init())
  {
    Serial.println("AT+LOG_I=Wifi initialization error!");
    return;
  }

  if (!tcp_ctrl_init())
  {
    Serial.println("AT+LOG_I=TCP initialization error!");
    return;
  }

  if (!queues_init())
  {
    Serial.println("AT+LOG_I=Queues initialization error!");
    return;
  }

  if (!mutexes_init())
  {
    Serial.println("AT+LOG_I=Mutexes initialization error!");
    return;
  }

  // Utworzenie zadań FreeRTOS
  xTaskCreatePinnedToCore(canControlTask, "CAN Control Task", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(canRxTask, "CAN Rx Task", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(appTask, "App Task", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(tcpTxTask, "TCP Tx Task", 4096, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(serialRxTask, "Serial Rx Task", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(logTxTask, "Log Tx Task", 4096, NULL, 3, NULL, 1);
}

void loop()
{
  // Main loop is empty as we are using FreeRTOS tasks
}
