# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

CrowPanel is an IoT environmental monitoring system built around the **CrowPanel 7.0" HMI ESP32 Display** (800x480 capacitive TFT-LCD, ESP32-S3-WROOM-1-N4R8, dual-core LX6, 240MHz, 4MB Flash, 8MB PSRAM, WiFi + BLE 5.0).

Hardware spec: `doc/panel_spec.md`

## System Architecture

Two PlatformIO projects communicate via MQTT (HiveMQ Cloud, TLS on port 8883):

1. **Sensor Node** (`sensor_node/`) — ESP12 (ESP8266) with 3x DHT22 sensors. Two outdoor sensors are averaged into one reading; one enclosure sensor reports separately. Publishes every 5 minutes. **Temperatures are published in Fahrenheit.**
2. **Display Node** (`display_node/`) — CrowPanel ESP32-S3 with 7" LVGL display. Subscribes to MQTT, displays comprehensive dashboard with time, weather, sensor data, garage status, and historical charts.

**MQTT Topics:**
- `crowpanel/outdoor` — averaged outdoor temp/humidity in Fahrenheit `{"t":72.5,"h":48.7}`
- `crowpanel/enclosure` — enclosure temp/humidity in Fahrenheit `{"t":78.1,"h":35.4}`
- `crowpanel/garage` — garage door status `{"status":"open"}` or `{"status":"closed"}`

## Build Commands

Both projects use PlatformIO with the Arduino framework.

```bash
# Sensor Node (ESP12)
cd sensor_node
pio run                          # Build
pio run --target upload          # Flash
pio device monitor --baud 115200 # Serial monitor

# Display Node (CrowPanel ESP32-S3)
cd display_node
pio run                          # Build
pio run --target upload          # Flash
pio device monitor --baud 9600   # Serial monitor
```

## Credentials

Both projects require `include/credentials.h` (gitignored). Copy from `include/credentials.h.template` and fill in WiFi and HiveMQ Cloud credentials.

## Sensor Node (`sensor_node/`)

- **Platform:** ESP8266 (`esp12e` board)
- **DHT22 pins:** Outdoor #1 GPIO 4 (D2), Outdoor #2 GPIO 5 (D1), Enclosure GPIO 14 (D5)
- **Libraries:** PubSubClient (MQTT), DHTesp (sensor reading)
- **Key file:** `src/main.cpp` — WiFi/MQTT connection, sensor reading with outdoor averaging and fallback, Celsius to Fahrenheit conversion, 5-min publish loop
- **Temperature units:** All readings converted to Fahrenheit before MQTT publish (formula: `F = C * 9/5 + 32`)

## Display Node (`display_node/`)

- **Platform:** ESP32-S3 with custom board def (`esp32-s3-devkitc-1-myboard.json`) and partition table (`huge_app.csv`), both copied from the example project
- **Stack:** Arduino → LVGL 8.3.6 (UI) → LovyanGFX 1.1.12 (display driver) → ESP32-S3 hardware
- **Libraries:** LVGL, LovyanGFX, TAMC_GT911, PubSubClient, ArduinoJson
- **Key files:**
  - `src/main.cpp` — LovyanGFX display driver (16-bit RGB parallel bus), LVGL init, touch input, WiFi/MQTT connection with `mqttCallback`, NTP time sync (EST UTC-5), time display updates
  - `src/ui.cpp` — Hand-written LVGL UI with dashboard layout (clock, sensors, charts, garage, forecast/calendar placeholders). Charts use `lv_chart` with 72-point sliding window (6h at 5-min intervals), values scaled x10
  - `src/fonts/` — Custom fonts: `ui_font_Big_Number` (72px Montserrat Light for clock), `ui_font_Bold_Font` (16px Montserrat Bold for labels)
  - `include/ui.h` — UI API: `ui_update_time()`, `ui_update_outdoor_temp()`, `ui_update_enclosure()`, `ui_chart_add_point()`, `ui_update_garage_status()`
  - `include/touch.h` — GT911 capacitive touch driver (I2C SDA=19, SCL=20), copied from example

**Dashboard Layout (800x480):**
```
+----------------------------------------------------------+
| Clock Panel (400x170)       | Calendar  | 5 Forecast    |
| - Time (72px Big Number)    | Sidebar   | Panels        |
| - Date                      | (150x90)  | (125x85 each) |
| - Outdoor temp (48px)       |           |               |
| - Weather icon (placeholder)|           |               |
| - Enclosure label           |           |               |
+----------------------------------------------------------+
| Combined Chart (795x110) - Outdoor temp + humidity       |
| Dual-axis: 0-120°F (temp), 0-100% (humidity)            |
+----------------------------------------------------------+
| Garage Panel (800x60) - Status badge with color coding  |
+----------------------------------------------------------+
```

**Color scheme:** Deep blue background (#1A1A50), dark panels (#16213E), red=outdoor temp, teal=humidity/enclosure, green=closed, red=open, gold=placeholders

**NTP Time:** Syncs on startup with `pool.ntp.org`, updates display every second. Timezone configured in `main.cpp` (default: EST UTC-5 with DST)

**Phase 2 Features (placeholders ready):** Weather forecast API integration, calendar API integration, touch controls for interactivity

## Example Project (`examples/CrowPanel_ESP32_7.0/`)

Vendor-provided example with DHT20 sensor and SquareLine Studio-generated UI. Used as reference for display driver config and pin mappings. UI code in `src/ui*.c` is auto-generated — don't hand-edit.
