#include "mqttClient.h"
#include <WiFi.h>
#include <mqtt_client.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "ui/vars.h"

extern void lvgl_port_lock(int timeout_ms);
extern void lvgl_port_unlock(void);

namespace mqttClient
{
    static char host[128] = {0};
    static uint16_t port = 8883;
    static char username[64] = {0};
    static char password[128] = {0};
    static char *caCertPem = NULL;

    static esp_mqtt_client_handle_t client = NULL;
    static volatile bool _connected = false;

    // Queue for passing received messages from MQTT task to main loop
    struct MqttMessage
    {
        char topic[128];
        char payload[512];
        int payloadLen;
    };
    static QueueHandle_t incomingQueue = NULL;

    // Forward declarations
    static void processMessage(const char *topic, const byte *payload, unsigned int length);
    static void processGnssMode(int mode);

    // --- ESP-IDF MQTT event handler (runs in MQTT task) ---

    static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                   int32_t event_id, void *event_data)
    {
        esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

        switch ((esp_mqtt_event_id_t)event_id)
        {
        case MQTT_EVENT_CONNECTED:
            debugln("[MQTT] Connected");
            _connected = true;
            set_var_websocket_connected(true);

            esp_mqtt_client_subscribe(client, "local/lights/+/status", 0);
            esp_mqtt_client_subscribe(client, "local/energy/status", 0);
            esp_mqtt_client_subscribe(client, "local/airquality/temphumid", 0);
            esp_mqtt_client_subscribe(client, "local/gps/latlon", 0);
            esp_mqtt_client_subscribe(client, "local/gps/alt", 0);
            esp_mqtt_client_subscribe(client, "local/gps/details", 0);
            debugln("[MQTT] Subscribed to all topics");
            break;

        case MQTT_EVENT_DISCONNECTED:
            debugln("[MQTT] Disconnected");
            _connected = false;
            set_var_websocket_connected(false);
            break;

        case MQTT_EVENT_DATA:
        {
            // Skip fragmented messages (our payloads are small, shouldn't happen)
            if (event->current_data_offset != 0 || event->data_len != event->total_data_len)
            {
                debugln("[MQTT] Skipping fragmented message");
                break;
            }

            MqttMessage msg = {};
            int topicLen = (event->topic_len < (int)sizeof(msg.topic) - 1) ? event->topic_len : (int)sizeof(msg.topic) - 1;
            int dataLen = (event->data_len < (int)sizeof(msg.payload) - 1) ? event->data_len : (int)sizeof(msg.payload) - 1;
            memcpy(msg.topic, event->topic, topicLen);
            msg.topic[topicLen] = '\0';
            memcpy(msg.payload, event->data, dataLen);
            msg.payload[dataLen] = '\0';
            msg.payloadLen = dataLen;

            debugf("[MQTT] RX @%lums: %s (%d bytes)\n", millis(), msg.topic, msg.payloadLen);

            if (incomingQueue)
                xQueueSend(incomingQueue, &msg, 0);
            break;
        }

        case MQTT_EVENT_ERROR:
            if (event->error_handle)
            {
                debugf("[MQTT] Error type: %d\n", event->error_handle->error_type);
            }
            break;

        default:
            break;
        }
    }

    bool hasConfig()
    {
        return strlen(host) > 0 && strlen(username) > 0 && strlen(password) > 0;
    }

    void loadSettings(Preferences &prefs)
    {
        String h = prefs.getString("mqttHost", "");
        if (h.length() > 0)
        {
            strncpy(host, h.c_str(), sizeof(host) - 1);
            debugf("[MQTT] Host: %s\n", host);
        }

        port = prefs.getUShort("mqttPort", 8883);
        debugf("[MQTT] Port: %d\n", port);

        String u = prefs.getString("mqttUser", "");
        if (u.length() > 0)
        {
            strncpy(username, u.c_str(), sizeof(username) - 1);
            debugf("[MQTT] User: %s\n", username);
        }

        String p = prefs.getString("mqttPass", "");
        if (p.length() > 0)
        {
            strncpy(password, p.c_str(), sizeof(password) - 1);
            debugln("[MQTT] Password loaded");
        }

        // Load CA cert from NVS into PSRAM
        String cert = prefs.getString("mqttCaCert", "");
        if (cert.length() > 0)
        {
            if (caCertPem)
                heap_caps_free(caCertPem);
            size_t certLen = cert.length() + 1;
            caCertPem = (char *)heap_caps_malloc(certLen, MALLOC_CAP_SPIRAM);
            if (caCertPem)
            {
                memcpy(caCertPem, cert.c_str(), certLen);
                debugf("[MQTT] CA cert loaded (%d bytes)\n", cert.length());
            }
            else
            {
                debugln("[MQTT] Failed to allocate PSRAM for CA cert");
            }
        }

        // Update UI status
        set_var_server_url(host);
        set_var_api_key_configured(strlen(username) > 0);
    }

    void connect()
    {
        if (!hasConfig())
        {
            debugf("[MQTT] Missing config - host:%s user:%s pass:%s\n",
                   strlen(host) > 0 ? "ok" : "MISSING",
                   strlen(username) > 0 ? "ok" : "MISSING",
                   strlen(password) > 0 ? "ok" : "MISSING");
            return;
        }

        // Create incoming message queue if not already created
        if (!incomingQueue)
            incomingQueue = xQueueCreate(16, sizeof(MqttMessage));

        // Build URI
        char uri[192];
        snprintf(uri, sizeof(uri), "mqtts://%s:%d", host, port);

        // Build client ID from MAC
        uint8_t mac[6];
        WiFi.macAddress(mac);
        static char clientId[32];
        snprintf(clientId, sizeof(clientId), "tc-remote-%02x%02x", mac[4], mac[5]);

        debugf("[MQTT] Connecting to %s as %s...\n", uri, username);

        // Destroy previous client if reconnecting
        if (client)
        {
            esp_mqtt_client_stop(client);
            esp_mqtt_client_destroy(client);
            client = NULL;
        }

        esp_mqtt_client_config_t mqtt_cfg = {};
        mqtt_cfg.uri = uri;
        mqtt_cfg.username = username;
        mqtt_cfg.password = password;
        mqtt_cfg.client_id = clientId;
        mqtt_cfg.buffer_size = 1024;
        mqtt_cfg.keepalive = 30;

        if (caCertPem)
        {
            mqtt_cfg.cert_pem = caCertPem;
            mqtt_cfg.skip_cert_common_name_check = true;
            debugln("[MQTT] Using CA cert (skip hostname check)");
        }
        else
        {
            debugln("[MQTT] No CA cert, connecting with TLS (skip verification)");
        }

        client = esp_mqtt_client_init(&mqtt_cfg);
        if (!client)
        {
            debugln("[MQTT] Failed to init client");
            return;
        }

        esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                       mqtt_event_handler, NULL);
        esp_err_t err = esp_mqtt_client_start(client);
        if (err != ESP_OK)
        {
            debugf("[MQTT] Failed to start client: %d\n", err);
        }
    }

    void disconnect()
    {
        if (client)
        {
            esp_mqtt_client_disconnect(client);
        }
        _connected = false;
        set_var_websocket_connected(false);
    }

    bool isConnected()
    {
        return _connected;
    }

    void loop()
    {
        if (!incomingQueue)
            return;

        MqttMessage msg;
        while (xQueueReceive(incomingQueue, &msg, 0) == pdTRUE)
        {
            // Take LVGL mutex — set_var_* functions call LVGL APIs
            lvgl_port_lock(-1);
            processMessage(msg.topic, (byte *)msg.payload, msg.payloadLen);
            lvgl_port_unlock();
        }
    }

    void publishLightCommand(int id, int state)
    {
        if (!_connected || !client)
        {
            debugln("[MQTT] Not connected, cannot publish");
            return;
        }

        char topic[64];
        snprintf(topic, sizeof(topic), "local/lights/%d/command", id);

        char payload[32];
        snprintf(payload, sizeof(payload), "{\"state\":%d}", state);

        debugf("[MQTT] Publishing %s -> %s\n", topic, payload);
        esp_mqtt_client_publish(client, topic, payload, 0, 0, 0);
    }

    // --- GNSS mode helper ---

    static void processGnssMode(int mode)
    {
        switch (mode)
        {
        case 1:
            set_var_gnss_mode("GPS");
            break;
        case 2:
            set_var_gnss_mode("Beidou");
            break;
        case 3:
            set_var_gnss_mode("GPS + Beidou");
            break;
        case 4:
            set_var_gnss_mode("GLONASS");
            break;
        case 5:
            set_var_gnss_mode("GPS + GLONASS");
            break;
        case 6:
            set_var_gnss_mode("Beidou + GLONASS");
            break;
        case 7:
            set_var_gnss_mode("GPS + Beidou + GLONASS");
            break;
        default:
            set_var_gnss_mode("Unknown");
            break;
        }
    }

    // --- Process incoming MQTT message ---

    static void processMessage(const char *topic, const byte *payload, unsigned int length)
    {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload, length);
        if (err)
        {
            debugf("[MQTT] JSON parse error: %s\n", err.c_str());
            return;
        }

        // local/lights/{id}/status
        if (strncmp(topic, "local/lights/", 13) == 0)
        {
            const char *idStr = topic + 13;
            int id = atoi(idStr);
            int state = doc["state"] | 0;
            int brightness = doc["brightness"] | 0;
            int value = (state > 0) ? ((brightness > 0) ? brightness : 1) : 0;

            switch (id)
            {
            case 1:
                set_var_pdm01_device01_status(value);
                break;
            case 2:
                set_var_pdm01_device02_status(value);
                break;
            case 3:
                set_var_pdm01_device03_status(value);
                break;
            case 4:
                set_var_pdm01_device04_status(value);
                break;
            case 5:
                set_var_pdm01_device05_status(value);
                break;
            case 6:
                set_var_pdm01_device06_status(value);
                break;
            case 7:
                set_var_pdm01_device07_status(value);
                break;
            case 8:
                set_var_pdm01_device08_status(value);
                break;
            default:
                debugf("[MQTT] Unknown light id: %d\n", id);
                break;
            }
        }
        else if (strcmp(topic, "local/energy/status") == 0)
        {
            float battPercent = doc["battery_percent"] | 0.0f;
            float battVoltage = doc["battery_voltage"] | 0.0f;
            float solarWatts = doc["solar_watts"] | 0.0f;
            const char *chargeType = doc["charge_type"] | "";
            set_var_battery_soc_percentage((int32_t)battPercent);
            set_var_battery_voltage(battVoltage);
            set_var_solar_wattage((int32_t)solarWatts);
            set_var_solar_status(chargeType);
        }
        else if (strcmp(topic, "local/airquality/temphumid") == 0)
        {
            float tempC = doc["temperature"] | 0.0f;
            float humidity = doc["humidity"] | 0.0f;
            float tempF = tempC * 9.0f / 5.0f + 32.0f;
            set_var_current_temperature_value(tempF);
            set_var_current_humidity_value(humidity);
        }
        else if (strcmp(topic, "local/gps/latlon") == 0)
        {
            float lat = doc["latitude"] | 0.0f;
            float lon = doc["longitude"] | 0.0f;
            set_var_current_latitude(lat);
            set_var_current_longitude(lon);
        }
        else if (strcmp(topic, "local/gps/alt") == 0)
        {
            float altFeet = doc["altitudeFeet"] | 0.0f;
            set_var_current_altitude_value(altFeet);
        }
        else if (strcmp(topic, "local/gps/details") == 0)
        {
            int sats = doc["numberOfSatellites"] | 0;
            float speed = doc["speedOverGround"] | 0.0f;
            float course = doc["courseOverGround"] | 0.0f;
            int gnssMode = doc["gnssMode"] | 0;
            set_var_number_of_satellites(sats);
            set_var_current_speed_value((int32_t)speed);
            set_var_current_course_over_ground(course);
            processGnssMode(gnssMode);
        }
        else
        {
            debugf("[MQTT] Unhandled topic: %s\n", topic);
        }
    }
}
