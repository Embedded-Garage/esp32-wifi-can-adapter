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
        preferences.putString("ssid", default_ssid);
    }

    if (preferences.isKey("pass"))
    {
        preferences.getString("pass", password, sizeof(password));
    }
    else
    {
        strcpy(password, default_password);
        preferences.putString("pass", default_password);
    }

    // Połączenie z WiFi
    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000)
    {
        delay(1000);
        Serial.println("AT+LOG_I=Connecting to WiFi...");
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("AT+LOG_W=Failed to connect to WiFi. Starting Access Point.");
        WiFi.softAP("EmbeddedGarage_CAN_tool");
        Serial.print("AT+LOG_I=Access Point IP: ");
        Serial.println(WiFi.softAPIP());
    }
    else
    {
        Serial.println("AT+LOG_I=Connected to WiFi");
        Serial.print("AT+LOG_I=IP address: ");
        Serial.println(WiFi.localIP());
    }

    return true;
}