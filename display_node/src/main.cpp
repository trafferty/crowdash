#include <Arduino.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "credentials.h"
#include "app_ui.h"

// ============================================================
// LovyanGFX display driver (from CrowPanel example)
// ============================================================
class LGFX : public lgfx::LGFX_Device
{
public:
    lgfx::Bus_RGB     _bus_instance;
    lgfx::Panel_RGB   _panel_instance;

    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;

            cfg.pin_d0  = GPIO_NUM_15; // B0
            cfg.pin_d1  = GPIO_NUM_7;  // B1
            cfg.pin_d2  = GPIO_NUM_6;  // B2
            cfg.pin_d3  = GPIO_NUM_5;  // B3
            cfg.pin_d4  = GPIO_NUM_4;  // B4

            cfg.pin_d5  = GPIO_NUM_9;  // G0
            cfg.pin_d6  = GPIO_NUM_46; // G1
            cfg.pin_d7  = GPIO_NUM_3;  // G2
            cfg.pin_d8  = GPIO_NUM_8;  // G3
            cfg.pin_d9  = GPIO_NUM_16; // G4
            cfg.pin_d10 = GPIO_NUM_1;  // G5

            cfg.pin_d11 = GPIO_NUM_14; // R0
            cfg.pin_d12 = GPIO_NUM_21; // R1
            cfg.pin_d13 = GPIO_NUM_47; // R2
            cfg.pin_d14 = GPIO_NUM_48; // R3
            cfg.pin_d15 = GPIO_NUM_45; // R4

            cfg.pin_henable = GPIO_NUM_41;
            cfg.pin_vsync   = GPIO_NUM_40;
            cfg.pin_hsync   = GPIO_NUM_39;
            cfg.pin_pclk    = GPIO_NUM_0;
            cfg.freq_write  = 15000000;

            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 40;
            cfg.hsync_pulse_width = 48;
            cfg.hsync_back_porch  = 40;

            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 1;
            cfg.vsync_pulse_width = 31;
            cfg.vsync_back_porch  = 13;

            cfg.pclk_active_neg   = 1;
            cfg.de_idle_high      = 0;
            cfg.pclk_idle_high    = 0;

            _bus_instance.config(cfg);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width  = 800;
            cfg.memory_height = 480;
            cfg.panel_width  = 800;
            cfg.panel_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            _panel_instance.config(cfg);
        }
        _panel_instance.setBus(&_bus_instance);
        setPanel(&_panel_instance);
    }
};

LGFX lcd;

// Touch (must be included after lcd declaration)
#define TFT_BL 2
#include "touch.h"

// ============================================================
// LVGL display buffer and driver
// ============================================================
static lv_color_t disp_draw_buf[800 * 480 / 15];
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    lcd.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t*)&color_p->full);
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    if (touch_has_signal()) {
        if (touch_touched()) {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = touch_last_x;
            data->point.y = touch_last_y;
        } else if (touch_released()) {
            data->state = LV_INDEV_STATE_REL;
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
    delay(15);
}

// ============================================================
// MQTT
// ============================================================
WiFiClientSecure wifiClientSecure;
PubSubClient mqttClient(wifiClientSecure);

// ============================================================
// NTP Time Configuration
// ============================================================
// POSIX TZ string: CST6CDT handles DST automatically (Mar 2nd Sun -> Nov 1st Sun)
const char* NTP_TZ     = "CST6CDT,M3.2.0,M11.1.0";
const char* NTP_SERVER = "pool.ntp.org";
static bool timeInitialized = false;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    char json[length + 1];
    memcpy(json, payload, length);
    json[length] = '\0';

    Serial.print("MQTT: ");
    Serial.print(topic);
    Serial.print(" -> ");
    Serial.println(json);

    // Handle garage door status
    if (strstr(topic, "garage")) {
        StaticJsonDocument<128> doc;
        DeserializationError err = deserializeJson(doc, json);
        if (err != DeserializationError::Ok) {
            Serial.print("Garage JSON parse error: ");
            Serial.println(err.c_str());
        } else {
            const char* status = doc["event"];
            const char* ts     = doc["ts"] | "";
            if (status) {
                Serial.print("Garage status: ");
                Serial.println(status);
                ui_update_garage_status(status);
                if (*ts) ui_update_garage_time(ts);
            } else {
                Serial.println("Garage JSON missing 'event' field");
            }
        }
        return;
    }

    // Handle sensor readings (now in Fahrenheit from sensor)
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) {
        Serial.println("JSON parse error");
        return;
    }

    float temp = doc["t"];  // Now in Fahrenheit from sensor
    float hum  = doc["h"];
    const char* ts = doc["ts"] | "";

    int sensorIdx = -1;
    if (strstr(topic, "outdoor")) {
        sensorIdx = 0;
        ui_update_outdoor_temp(temp);  // Large display
        ui_chart_add_point(temp, hum); // Chart (outdoor only)
        if (*ts) ui_update_sensor_timestamp(ts);
    } else if (strstr(topic, "enclosure")) {
        sensorIdx = 1;
        ui_update_enclosure(temp, hum);  // Small label only
    }

    if (sensorIdx >= 0) {
        ui_update_current(sensorIdx, temp, hum);  // Keep for compatibility
    }
}

void connectWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.print("Connected, IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi connection failed");
    }
}

void initNTP() {
    Serial.print("Syncing time with NTP...");
    configTzTime(NTP_TZ, NTP_SERVER);

    struct tm timeinfo;
    int attempts = 0;
    while (!getLocalTime(&timeinfo) && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (attempts < 20) {
        Serial.println(" synchronized");
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
        timeInitialized = true;
    } else {
        Serial.println(" FAILED");
    }
}

void connectMQTT() {
    int attempts = 0;
    while (!mqttClient.connected() && attempts < 5) {
        Serial.print("Connecting to MQTT...");
        if (mqttClient.connect("crowpanel-display", MQTT_USER, MQTT_PASSWORD)) {
            Serial.println("connected");
            mqttClient.subscribe("crowpanel/outdoor");
            mqttClient.subscribe("crowpanel/enclosure");
            mqttClient.subscribe("crowpanel/garage");
        } else {
            attempts++;
            Serial.print("failed, rc=");
            Serial.println(mqttClient.state());
            delay(3000);
        }
    }
}

// ============================================================
// Setup and Loop
// ============================================================
void setup() {
    Serial.begin(9600);
    Serial.println("\nCrowPanel Display Node starting...");

    // GPIO 38 LOW (per example)
    pinMode(38, OUTPUT);
    digitalWrite(38, LOW);

    // Init display
    lcd.begin();
    lcd.fillScreen(TFT_BLACK);
    delay(200);

    // Init LVGL
    lv_init();
    touch_init();

    uint32_t screenWidth = lcd.width();
    uint32_t screenHeight = lcd.height();
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * screenHeight / 15);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // Backlight
    ledcSetup(1, 300, 8);
    ledcAttachPin(TFT_BL, 1);
    ledcWrite(1, 255);

    // Build UI
    ui_init();
    ui_app_init();
    lv_timer_handler();

    // WiFi + MQTT
    connectWiFi();
    initNTP();  // Initialize NTP after WiFi connects
    wifiClientSecure.setInsecure();
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);
    connectMQTT();
}

static uint32_t lv_tick_prev = 0;

void loop() {
    // Feed LVGL tick — required for UI updates
    uint32_t now = millis();
    lv_tick_inc(now - lv_tick_prev);
    lv_tick_prev = now;

    // Update time display every second
    static unsigned long lastTimeUpdate = 0;
    if (timeInitialized && millis() - lastTimeUpdate >= 1000) {
        lastTimeUpdate = millis();

        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStr[16];
            char dateStr[32];
            strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
            strftime(dateStr, sizeof(dateStr), "%A, %b %d", &timeinfo);
            ui_update_time(timeStr, dateStr);
        }
    }

    // Reconnect if needed
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop();

    lv_timer_handler();
    delay(5);
}
