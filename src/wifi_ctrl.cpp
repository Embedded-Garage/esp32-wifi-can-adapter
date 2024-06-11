#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

#include "wifi_ctrl.h"

const char *default_ssid = "miki3";
const char *default_password = "mikimiki";
char ssid[32];
char password[64];

bool wifi_ctrl_init()
{
    Preferences preferences;

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

    return true;
}