#pragma once
#include "globals.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <Preferences.h>

namespace wifiHelper
{
    extern const int MAX_SCAN_RESULTS;

    void initialize(Preferences &prefs);
    bool isConnected();
    bool hasCredentials();
    String getIPAddress();
    String getSSID();
    void readMacAddress();
    int scanNetworks();
    String getScanResult(int index);
    int getScanRSSI(int index);
    bool connectToNetwork(const char *ssid, const char *password, Preferences &prefs);
    void checkReconnect();
}
