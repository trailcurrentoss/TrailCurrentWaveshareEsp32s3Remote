#include "wifiHelper.h"
#include "ui/vars.h"

namespace wifiHelper
{
    const int MAX_SCAN_RESULTS = 20;
    static const unsigned long RECONNECT_INTERVAL_MS = 10000;

    static char savedSSID[64] = {0};
    static char savedPassword[64] = {0};
    static bool _connecting = false;
    static unsigned long _lastReconnectAttempt = 0;

    void readMacAddress()
    {
        uint8_t baseMac[6];
        char buffer[100];
        esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
        if (ret == ESP_OK)
        {
            snprintf(buffer, sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x",
                     baseMac[0], baseMac[1], baseMac[2],
                     baseMac[3], baseMac[4], baseMac[5]);
            debugf("[WiFi] MAC address: %s\n", buffer);
            set_var_mcu_mac_address(buffer);
        }
        else
        {
            debugln("[WiFi] Failed to read MAC address");
        }
    }

    bool isConnected()
    {
        return WiFi.status() == WL_CONNECTED;
    }

    String getIPAddress()
    {
        return WiFi.localIP().toString();
    }

    String getSSID()
    {
        return String(savedSSID);
    }

    bool hasCredentials()
    {
        return strlen(savedSSID) > 0;
    }

    bool connectToNetwork(const char *ssid, const char *password, Preferences &prefs)
    {
        debugf("[WiFi] Connecting to: %s\n", ssid);
        _connecting = true;

        WiFi.disconnect(true);
        WiFi.begin(ssid, password);

        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000)
        {
            delay(250);
        }

        _connecting = false;

        if (WiFi.status() == WL_CONNECTED)
        {
            debugf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());

            strncpy(savedSSID, ssid, sizeof(savedSSID) - 1);
            savedSSID[sizeof(savedSSID) - 1] = '\0';
            strncpy(savedPassword, password, sizeof(savedPassword) - 1);
            savedPassword[sizeof(savedPassword) - 1] = '\0';

            prefs.putString("wifiSSID", ssid);
            prefs.putString("wifiPass", password);

            set_var_wifi_connected(true);
            set_var_wifi_ssid(ssid);
            set_var_connection_status_text("Connected");
            return true;
        }
        else
        {
            debugln("[WiFi] Connection failed");
            set_var_wifi_connected(false);
            set_var_connection_status_text("Connection failed");
            return false;
        }
    }

    int scanNetworks()
    {
        debugln("[WiFi] Starting network scan...");
        set_var_connection_status_text("Scanning...");

        // Ensure clean STA mode before scanning
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(false);
        delay(500);

        int n = WiFi.scanNetworks(false, true, false, 300);
        debugf("[WiFi] scanNetworks returned: %d\n", n);

        // If first scan fails, retry once
        if (n <= 0)
        {
            debugln("[WiFi] Retrying scan...");
            WiFi.scanDelete();
            delay(1000);
            n = WiFi.scanNetworks(false, false, false, 500);
            debugf("[WiFi] Retry returned: %d\n", n);
        }

        for (int i = 0; i < n; i++)
        {
            debugf("[WiFi]   %d: '%s' (%d dBm) ch:%d\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
        }
        return n;
    }

    String getScanResult(int index)
    {
        if (index >= 0 && index < WiFi.scanComplete())
        {
            return WiFi.SSID(index);
        }
        return "";
    }

    int getScanRSSI(int index)
    {
        if (index >= 0 && index < WiFi.scanComplete())
        {
            return WiFi.RSSI(index);
        }
        return 0;
    }

    void initialize(Preferences &prefs)
    {
        WiFi.mode(WIFI_STA);

        String ssid = prefs.getString("wifiSSID", "");
        String pass = prefs.getString("wifiPass", "");

        if (ssid.length() > 0)
        {
            strncpy(savedSSID, ssid.c_str(), sizeof(savedSSID) - 1);
            strncpy(savedPassword, pass.c_str(), sizeof(savedPassword) - 1);

            debugf("[WiFi] Found saved credentials for: %s\n", savedSSID);
            WiFi.begin(savedSSID, savedPassword);

            unsigned long startAttempt = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000)
            {
                delay(250);
            }

            if (WiFi.status() == WL_CONNECTED)
            {
                debugf("[WiFi] Auto-connected! IP: %s\n", WiFi.localIP().toString().c_str());
                set_var_wifi_connected(true);
                set_var_wifi_ssid(savedSSID);
                set_var_connection_status_text("Connected");
            }
            else
            {
                debugln("[WiFi] Auto-connect failed, will retry");
                set_var_wifi_connected(false);
                set_var_connection_status_text("Disconnected");
            }
        }
        else
        {
            debugln("[WiFi] No saved credentials found");
            set_var_wifi_connected(false);
            set_var_connection_status_text("Not configured");
        }

        readMacAddress();
    }

    void checkReconnect()
    {
        if (!hasCredentials() || isConnected() || _connecting)
        {
            return;
        }

        unsigned long now = millis();
        if (now - _lastReconnectAttempt >= RECONNECT_INTERVAL_MS)
        {
            _lastReconnectAttempt = now;
            debugln("[WiFi] Attempting reconnect...");
            WiFi.disconnect(true);
            WiFi.begin(savedSSID, savedPassword);
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            debugln("[WiFi] Reconnected!");
            set_var_wifi_connected(true);
            set_var_connection_status_text("Connected");
        }
    }
}
