#include <Arduino.h>
#include <AsyncTCP.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <vector>

#include "app.h"
#include "queues.h"
#include "mutexes.h"
#include "statistics.h"

AsyncServer *server = NULL;
std::vector<AsyncClient *> clients;

void onClientConnected(void *arg, AsyncClient *client);
void onClientDisconnected(void *arg, AsyncClient *client);
void onClientData(void *arg, AsyncClient *client, void *data, size_t len);

bool tcp_ctrl_init(void)
{
    // Utworzenie serwera TCP
    server = new AsyncServer(1234); // port 1234
    server->onClient(&onClientConnected, server);
    server->begin();

    return true;
}

void tcp_ctrl_sendToAllClients(char data[], int len)
{
    xSemaphoreTake(xClientsMutex, portMAX_DELAY);
    for (auto client : clients)
    {
        if (client->connected())
        {
            client->write(data, len);
        }
    }
    xSemaphoreGive(xClientsMutex);
}

void onClientConnected(void *arg, AsyncClient *client)
{
    Serial.println("AT+LOG_I=New client connected");
    client->onData(&onClientData, client);
    client->onDisconnect(&onClientDisconnected, client);

    xSemaphoreTake(xClientsMutex, portMAX_DELAY);
    clients.push_back(client);
    xSemaphoreGive(xClientsMutex);
}

void onClientDisconnected(void *arg, AsyncClient *client)
{
    Serial.println("AT+LOG_I=Client disconnected");
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
        if (pdPASS != xQueueSend(appQueue, &app_msg, 0))
        {
            statistics.lost_frames_tcp_to_app++;
        }
    }
}
