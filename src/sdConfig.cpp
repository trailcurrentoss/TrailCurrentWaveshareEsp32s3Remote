#include "sdConfig.h"
#include "globals.h"
#include <SPI.h>
#include <SD.h>

// SD card SPI pins (Waveshare ESP32-S3-Touch-LCD-4.3)
#define SD_SPI_SCK 12
#define SD_SPI_MISO 13
#define SD_SPI_MOSI 11
#define SD_CS_EXPANDER_PIN 4

namespace sdConfig
{
    static bool parseLine(const String &line, String &key, String &value)
    {
        int eqIdx = line.indexOf('=');
        if (eqIdx <= 0)
            return false;

        key = line.substring(0, eqIdx);
        value = line.substring(eqIdx + 1);
        key.trim();
        value.trim();
        return key.length() > 0;
    }

    bool readConfigFromSD(ESP_IOExpander *expander, Preferences &prefs)
    {
        debugln("[SD] Attempting to read config from SD card...");

        // Pull CS LOW to select SD card
        expander->digitalWrite(SD_CS_EXPANDER_PIN, LOW);

        // Initialize SPI bus for SD card
        SPI.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI);

        // Mount SD card (CS is held LOW by expander, pass -1 as CS pin)
        if (!SD.begin(-1, SPI))
        {
            debugln("[SD] Failed to mount SD card");
            SPI.end();
            expander->digitalWrite(SD_CS_EXPANDER_PIN, HIGH);
            return false;
        }

        debugln("[SD] SD card mounted");

        // Open config file
        File configFile = SD.open("/config.env", FILE_READ);
        if (!configFile)
        {
            debugln("[SD] config.env not found");
            SD.end();
            SPI.end();
            expander->digitalWrite(SD_CS_EXPANDER_PIN, HIGH);
            return false;
        }

        debugln("[SD] Reading config.env...");

        // Clear all connection-related NVS keys so SD card is the single source of truth
        prefs.remove("wifiSSID");
        prefs.remove("wifiPass");
        prefs.remove("mqttHost");
        prefs.remove("mqttPort");
        prefs.remove("mqttUser");
        prefs.remove("mqttPass");
        prefs.remove("mqttCaCert");
        // Remove legacy keys from old firmware versions
        prefs.remove("serverUrl");
        prefs.remove("apiKey");
        debugln("[SD] Cleared old NVS connection settings");

        bool foundWifi = false;

        while (configFile.available())
        {
            String line = configFile.readStringUntil('\n');
            line.trim();

            // Skip empty lines and comments
            if (line.length() == 0 || line.startsWith("#"))
                continue;

            String key, value;
            if (!parseLine(line, key, value))
                continue;

            if (key == "WIFI_SSID" && value.length() > 0)
            {
                prefs.putString("wifiSSID", value.c_str());
                debugf("[SD] WiFi SSID: %s\n", value.c_str());
                foundWifi = true;
            }
            else if (key == "WIFI_PWD")
            {
                prefs.putString("wifiPass", value.c_str());
                debugln("[SD] WiFi password set");
            }
            else if (key == "MQTT_HOST" && value.length() > 0)
            {
                prefs.putString("mqttHost", value.c_str());
                debugf("[SD] MQTT Host: %s\n", value.c_str());
            }
            else if (key == "MQTT_PORT" && value.length() > 0)
            {
                int p = value.toInt();
                if (p > 0 && p <= 65535)
                {
                    prefs.putUShort("mqttPort", (uint16_t)p);
                    debugf("[SD] MQTT Port: %d\n", p);
                }
            }
            else if (key == "MQTT_USER" && value.length() > 0)
            {
                prefs.putString("mqttUser", value.c_str());
                debugf("[SD] MQTT User: %s\n", value.c_str());
            }
            else if (key == "MQTT_PASS" && value.length() > 0)
            {
                prefs.putString("mqttPass", value.c_str());
                debugln("[SD] MQTT password set");
            }
        }

        configFile.close();

        // Read CA certificate if present
        File certFile = SD.open("/ca.crt", FILE_READ);
        if (certFile)
        {
            String certPem = certFile.readString();
            certFile.close();
            if (certPem.length() > 0)
            {
                prefs.putString("mqttCaCert", certPem.c_str());
                debugf("[SD] CA cert stored (%d bytes)\n", certPem.length());
            }
        }
        else
        {
            debugln("[SD] ca.crt not found (optional)");
        }

        SD.end();
        SPI.end();
        expander->digitalWrite(SD_CS_EXPANDER_PIN, HIGH);

        debugf("[SD] Config read complete (WiFi found: %s)\n", foundWifi ? "yes" : "no");
        return foundWifi;
    }
}
