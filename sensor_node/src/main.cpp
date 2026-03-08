#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include "credentials.h"

// --- Pin assignments ---
#define DHT_OUTDOOR1_PIN 4   // GPIO4 (D2)
#define DHT_OUTDOOR2_PIN 5   // GPIO5 (D1)
#define DHT_ENCLOSURE_PIN 14 // GPIO14 (D5)

// --- Timing ---
#define PUBLISH_INTERVAL_MS 300000  // 5 minutes

// --- MQTT topics ---
static const char* TOPIC_OUTDOOR   = "crowpanel/outdoor";
static const char* TOPIC_ENCLOSURE = "crowpanel/enclosure";

// --- Globals ---
DHTesp dhtOutdoor1;
DHTesp dhtOutdoor2;
DHTesp dhtEnclosure;

BearSSL::WiFiClientSecure wifiClientSecure;
PubSubClient mqttClient(wifiClientSecure);

unsigned long lastPublish = 0;
int reconnectAttempts = 0;

// --- WiFi ---
void connectWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("Connected, IP: ");
    Serial.println(WiFi.localIP());
}

// --- MQTT ---
void connectMQTT() {
    unsigned long backoff = 1000;
    while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT...");
        if (mqttClient.connect("crowpanel-sensor", MQTT_USER, MQTT_PASSWORD)) {
            Serial.println("connected");
            reconnectAttempts = 0;
        } else {
            reconnectAttempts++;
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.print(", retrying in ");
            Serial.print(backoff / 1000);
            Serial.println("s");

            if (reconnectAttempts >= 10) {
                Serial.println("Too many failures, restarting...");
                ESP.restart();
            }

            delay(backoff);
            if (backoff < 30000) backoff *= 2;
        }
    }
}

// --- Publish helper ---
void publishJSON(const char* topic, float temp, float humidity) {
    char payload[32];
    snprintf(payload, sizeof(payload), "{\"t\":%.1f,\"h\":%.1f}", temp, humidity);
    Serial.print("Publish ");
    Serial.print(topic);
    Serial.print(": ");
    Serial.println(payload);
    mqttClient.publish(topic, payload, true);  // retained
}

// --- Read sensors and publish ---
void readAndPublish() {
    // Read outdoor sensors (DHT returns Celsius)
    float t1 = dhtOutdoor1.getTemperature();
    float h1 = dhtOutdoor1.getHumidity();
    bool ok1 = !isnan(t1) && !isnan(h1);

    float t2 = dhtOutdoor2.getTemperature();
    float h2 = dhtOutdoor2.getHumidity();
    bool ok2 = !isnan(t2) && !isnan(h2);

    // Average outdoor readings (fallback to single sensor if one fails)
    // Convert Celsius to Fahrenheit before publishing
    if (ok1 && ok2) {
        float tempC = (t1 + t2) / 2.0f;
        float tempF = tempC * 9.0f / 5.0f + 32.0f;
        float humAvg = (h1 + h2) / 2.0f;
        publishJSON(TOPIC_OUTDOOR, tempF, humAvg);
    } else if (ok1) {
        Serial.println("Outdoor sensor 2 failed, using sensor 1 only");
        float tempF = t1 * 9.0f / 5.0f + 32.0f;
        publishJSON(TOPIC_OUTDOOR, tempF, h1);
    } else if (ok2) {
        Serial.println("Outdoor sensor 1 failed, using sensor 2 only");
        float tempF = t2 * 9.0f / 5.0f + 32.0f;
        publishJSON(TOPIC_OUTDOOR, tempF, h2);
    } else {
        Serial.println("Both outdoor sensors failed, skipping outdoor publish");
    }

    // Read enclosure sensor (convert to Fahrenheit)
    float tE = dhtEnclosure.getTemperature();
    float hE = dhtEnclosure.getHumidity();
    if (!isnan(tE) && !isnan(hE)) {
        float tempF = tE * 9.0f / 5.0f + 32.0f;
        publishJSON(TOPIC_ENCLOSURE, tempF, hE);
    } else {
        Serial.println("Enclosure sensor failed, skipping publish");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nCrowPanel Sensor Node starting...");

    // Init DHT22 sensors
    dhtOutdoor1.setup(DHT_OUTDOOR1_PIN, DHTesp::DHT22);
    dhtOutdoor2.setup(DHT_OUTDOOR2_PIN, DHTesp::DHT22);
    dhtEnclosure.setup(DHT_ENCLOSURE_PIN, DHTesp::DHT22);

    // Connect WiFi
    connectWiFi();

    // Configure MQTT
    wifiClientSecure.setInsecure();  // Skip cert verification for now
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setBufferSize(256);
    connectMQTT();

    // Publish immediately on startup
    readAndPublish();
    lastPublish = millis();
}

void loop() {
    // Reconnect WiFi if needed
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost, reconnecting...");
        connectWiFi();
    }

    // Reconnect MQTT if needed
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop();

    // Publish on interval
    if (millis() - lastPublish >= PUBLISH_INTERVAL_MS) {
        readAndPublish();
        lastPublish = millis();
    }
}
