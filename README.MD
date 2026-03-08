# TrailCurrent Spotter

Touchscreen remote monitor for the [TrailCurrent](https://trailcurrent.com) modular vehicle system, built on the Waveshare ESP32-S3 4.3" touch display with LVGL GUI. Connects directly to the local MQTT broker over WiFi for real-time data and control.

## Hardware Overview

- **Display:** [Waveshare ESP32-S3 4.3" capacitive touchscreen](https://www.waveshare.com/product/arduino/boards-kits/esp32-s3/esp32-s3-touch-lcd-4.3.htm?aff_id=Trailcurrent) (800x480 RGB) — small form factor ideal for in-car usage where space is limited, and the ESP32-S3 provides more than sufficient performance for displaying real-time status alerts and alarms while driving
- **Microcontroller:** ESP32-S3 with 8MB flash, PSRAM (QIO OPI)
- **IO Expander:** CH422G I2C IO expander for display control
- **SD Card:** MicroSD slot (SPI on GPIO 11/12/13, CS via CH422G IO expander)
- **Communication:** WiFi — connects directly to the local Mosquitto MQTT broker (TLS on port 8883)
- **Key Features:**
  - Multi-page touch interface with bottom navigation bar
  - Real-time vehicle status via MQTT subscriptions
  - Light and accessory control via MQTT publish
  - SD card configuration — no on-screen credential entry needed
  - Configurable themes and screen timeout
  - Timezone-aware date/time display
  - User settings persisted in NVS (non-volatile storage)
  - 3D-printable enclosure with battery compartment

## GUI Pages

| Page | Function |
|------|----------|
| **Home** | Status dashboard with warning indicators (lights, water, power, doors, cabinets, stabilizers) |
| **Trailer** | Trailer-specific monitoring and controls |
| **Power** | Battery voltage, state of charge, shore power status, PDM device control |
| **Solar** | Solar charge controller status and wattage |
| **Air** | Temperature (F), humidity, and air quality readings |
| **Settings** | Display theme, screen timeout, gateway MAC address, timezone, connectivity, about/version |

The GUI is designed with [EEZ Studio](https://www.envox.hr/eez/studio/studio-introduction.html) and rendered using LVGL 8.4.0.

## Device Configuration

Configuration is loaded from a `config.env` file and optional `ca.crt` on a MicroSD card. This avoids typing long credentials on the small touch keyboard.

**Setup steps:**

1. Create a file called `config.env` on a MicroSD card (FAT32 formatted)
2. Add your configuration values:

```
WIFI_SSID=YourNetworkName
WIFI_PWD=YourWiFiPassword
MQTT_HOST=192.168.1.100
MQTT_PORT=8883
MQTT_USER=mqttuser
MQTT_PASS=mqttpassword
```

3. Optionally copy the Mosquitto CA certificate as `ca.crt` to the SD card root for TLS server verification
4. Insert the SD card into the device and power on (or restart)
5. The device reads the config and CA cert, saves both to NVS, and connects automatically
6. The SD card can be removed after the first boot — settings persist in NVS

**Notes:**
- `MQTT_HOST` should be an IP address (ESP32 does not resolve `.local` mDNS hostnames)
- `MQTT_PORT` defaults to 8883 if omitted
- TLS encryption is always used; if no `ca.crt` is provided, certificate verification is skipped but the connection remains encrypted
- If no configuration exists (no SD card and no saved NVS values), the device shows "Configuration Required — Insert SD card with config.env and ca.crt then restart"
- Re-inserting an SD card will clear all previous connection settings and replace them with the new values on next boot

## Communication Protocol

The remote connects directly to the Mosquitto MQTT broker over TLS (port 8883). No intermediate server is required for real-time operation.

**Subscribe (status updates from CAN bus devices via MQTT):**

| Topic | Data Fields |
|-------|-------------|
| `local/lights/+/status` | `state` (0/1), `brightness` (0-255) |
| `local/energy/status` | `battery_percent`, `battery_voltage`, `solar_watts`, `charge_type` |
| `local/airquality/temphumid` | `temperature` (Celsius — converted to F on device), `humidity` |
| `local/gps/latlon` | `latitude`, `longitude` |
| `local/gps/alt` | `altitudeFeet` |
| `local/gps/details` | `numberOfSatellites`, `speedOverGround`, `courseOverGround`, `gnssMode` |

**Publish (commands to devices):**

| Topic | Payload |
|-------|---------|
| `local/lights/{id}/command` | `{"state": 0\|1}` |

The MQTT client uses the ESP-IDF native `esp_mqtt_client` for event-driven, non-blocking message delivery.

## Firmware Setup

**Prerequisites:**
- [PlatformIO](https://platformio.org/) (CLI or IDE plugin)

**Build and upload:**
```bash
# Build firmware
pio run

# Upload to board (serial)
pio run -t upload
```

No secrets files are needed in the source. Device configuration (WiFi credentials, MQTT broker details) is loaded at runtime from an SD card — see [Device Configuration](#device-configuration) above.

## Case Design

A 3D-printable enclosure is included in the `CAD/` directory (FreeCAD format). The case accommodates a battery and physical buttons alongside the built-in touch interface.

![Case Design](ASSETS/CaseDesign.png)

## Project Structure

```
├── ASSETS/                       # Images and documentation assets
│   └── CaseDesign.png
├── CAD/                          # 3D-printable enclosure (FreeCAD)
│   └── TrailCurrentSpotter.FCStd
├── GUI/                          # EEZ Studio GUI project
│   └── TrailCurrentSpotter.eez-project
├── src/                          # Firmware source
│   ├── main.cpp                  # Application entry, LVGL setup, IO expander init, boot flow
│   ├── globals.h                 # Version, debug macros
│   ├── sdConfig.h / sdConfig.cpp # SD card config.env and ca.crt reader
│   ├── wifiHelper.h / .cpp       # WiFi connection management
│   ├── mqttClient.h / .cpp       # MQTT client (ESP-IDF native, TLS, pub/sub)
│   ├── actions.cpp               # UI event handlers (navigation, settings, themes, controls)
│   ├── vars.cpp                  # State variables, LVGL widget updates, time conversion
│   └── ui/                       # Generated LVGL UI code (from EEZ Studio — do not edit)
│       ├── screens.h / screens.c # Screen and widget object definitions
│       ├── styles.h / styles.c   # Theme styles
│       ├── vars.h                # Variable getter/setter declarations
│       ├── actions.h             # Action function declarations
│       ├── images.h / images.c   # Embedded images
│       └── fonts/                # Custom fonts (FontAwesome icons)
├── lib/                          # Local libraries
│   └── ESP32_Display_Panel/      # Waveshare display driver (ESP-IDF based)
├── 8MB_no_ota.csv                # Flash partition table
└── platformio.ini                # Build configuration
```

## Dependencies

| Library | Source | Purpose |
|---------|--------|---------|
| LVGL 8.4.0 | PlatformIO registry | GUI framework |
| ArduinoJson 7.x | PlatformIO registry | JSON parsing for MQTT messages |
| ESP-IDF MQTT | Built-in (ESP-IDF) | Native MQTT client with TLS support |
| ESP32_Display_Panel | Local (`lib/`) | Waveshare display and touch driver |
| SD / SPI | ESP32 Arduino built-in | SD card access for config.env and ca.crt |
| WiFi | ESP32 Arduino built-in | WiFi connectivity |

## License

MIT License - See LICENSE file for details.

This is open source hardware and software. You are free to use, modify, and distribute under the terms of the MIT license.

## Contributing

Improvements and contributions are welcome! Please submit issues or pull requests.
