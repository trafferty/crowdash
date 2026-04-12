#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include "credentials.h"
#include "sierra_wifi_defs.h"

#define version_str "2025_11_23: v2.2: Changed to ESPAsyncWebServer."

// Uncomment to use mobile IP/hostname instead of porch
#define MOBILE

#ifdef MOBILE
    static const int  IP_last_field = DHT22_MOBILE_TEMP_SERVER_IP_LAST_FIELD;
    static const char DNS_name[]    = DHT22_MOBILE_TEMP_SERVER_HOSTNAME;
    String sys_id = "Mobile unit";
#else
    static const int  IP_last_field = DHT22_PORCH_TEMP_SERVER_IP_LAST_FIELD;
    static const char DNS_name[]    = DHT22_PORCH_TEMP_SERVER_HOSTNAME;
    String sys_id = "Porch unit";
#endif

static const IPAddress staticIP (IP1, IP2, IP3, IP_last_field);
static const IPAddress gatewayIP(GW1, GW2, GW3, GW4);
static const IPAddress subnetIP (SN1, SN2, SN3, SN4);
static const IPAddress dnsIP    (DNS1, DNS2, DNS3, DNS4);


// --- Pin assignments ---
#define DHT_OUTDOOR1_PIN 4   // GPIO4 (D2)
#define DHT_OUTDOOR2_PIN 5   // GPIO5 (D1)
#define DHT_ENCLOSURE_PIN 14 // GPIO14 (D5)

// --- Timing ---
#define PUBLISH_INTERVAL_MS 300000   // 5 minutes
#define NTP_INTERVAL_MS     3600000  // 1 hour

// --- MQTT topics ---
static const char* TOPIC_OUTDOOR   = "crowpanel/outdoor";
static const char* TOPIC_ENCLOSURE = "crowpanel/enclosure";

// --- Globals ---
DHTesp dhtOutdoor1;
DHTesp dhtOutdoor2;
DHTesp dhtEnclosure;

BearSSL::WiFiClientSecure wifiClientSecure;
PubSubClient mqttClient(wifiClientSecure);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80);

unsigned long lastPublish   = 0;
unsigned long lastNTPUpdate = 0;
int reconnectAttempts = 0;

// Last known readings (for web display)
static float lastOutdoor1Temp   = 0.0f, lastOutdoor1Hum   = 0.0f;
static float lastOutdoor2Temp   = 0.0f, lastOutdoor2Hum   = 0.0f;
static float lastEnclosureTemp  = 0.0f, lastEnclosureHum  = 0.0f;
static float lastOutdoorAvgTemp = 0.0f, lastOutdoorAvgHum = 0.0f;

// Forward declarations
String buildTimeDateStr();
String CreateRootHTML();
String CreateTempDisplayHTML();

// --- WiFi ---
void connectWiFi() {
    Serial.printf("Connecting to WiFi with static IP (%s)\n", DNS_name);
    WiFi.config(staticIP, gatewayIP, subnetIP, dnsIP);
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

// --- NTP + Web Server ---
void setupOnline() {
    Serial.println("Starting NTP time client...");
    timeClient.begin();
    timeClient.setTimeOffset(-6 * 3600);  // CST (UTC-6)
    delay(1000);
    timeClient.update();
    Serial.println(timeClient.getFormattedTime());
    lastNTPUpdate = millis();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println(" - Handling request for /...");
        request->send(200, "text/html", CreateRootHTML());
    });
    server.on("/display_data", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println(" - Handling request for /display_data...");
        request->send(200, "text/html", CreateTempDisplayHTML());
    });

    server.begin();
    Serial.println("Web server started on port 80");
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

// --- NTP timestamp builder (ISO 8601) ---
String buildTimeDateStr() {
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime((time_t *)&epochTime);
    int yr  = ptm->tm_year + 1900;
    int mon = ptm->tm_mon  + 1;
    int day = ptm->tm_mday;
    String ts = String(yr) + "-";
    if (mon < 10) ts += "0";
    ts += String(mon) + "-";
    if (day < 10) ts += "0";
    ts += String(day) + "T";
    ts += timeClient.getFormattedTime();
    return ts;
}

// --- Publish helper (includes timestamp) ---
void publishJSON(const char* topic, float temp, float humidity) {
    String ts = buildTimeDateStr();
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"t\":%.1f,\"h\":%.1f,\"ts\":\"%s\"}",
             temp, humidity, ts.c_str());
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

    // Cache individual readings for web display (convert to Fahrenheit)
    if (ok1) { lastOutdoor1Temp = t1 * 9.0f / 5.0f + 32.0f; lastOutdoor1Hum = h1; }
    if (ok2) { lastOutdoor2Temp = t2 * 9.0f / 5.0f + 32.0f; lastOutdoor2Hum = h2; }

    // Average outdoor readings, fallback to single sensor if one fails
    if (ok1 && ok2) {
        float tempF  = (t1 + t2) / 2.0f * 9.0f / 5.0f + 32.0f;
        float humAvg = (h1 + h2) / 2.0f;
        lastOutdoorAvgTemp = tempF;
        lastOutdoorAvgHum  = humAvg;
        publishJSON(TOPIC_OUTDOOR, tempF, humAvg);
    } else if (ok1) {
        Serial.println("Outdoor sensor 2 failed, using sensor 1 only");
        float tempF = t1 * 9.0f / 5.0f + 32.0f;
        lastOutdoorAvgTemp = tempF; lastOutdoorAvgHum = h1;
        publishJSON(TOPIC_OUTDOOR, tempF, h1);
    } else if (ok2) {
        Serial.println("Outdoor sensor 1 failed, using sensor 2 only");
        float tempF = t2 * 9.0f / 5.0f + 32.0f;
        lastOutdoorAvgTemp = tempF; lastOutdoorAvgHum = h2;
        publishJSON(TOPIC_OUTDOOR, tempF, h2);
    } else {
        Serial.println("Both outdoor sensors failed, skipping outdoor publish");
    }

    // Read enclosure sensor
    float tE = dhtEnclosure.getTemperature();
    float hE = dhtEnclosure.getHumidity();
    if (!isnan(tE) && !isnan(hE)) {
        lastEnclosureTemp = tE * 9.0f / 5.0f + 32.0f;
        lastEnclosureHum  = hE;
        publishJSON(TOPIC_ENCLOSURE, lastEnclosureTemp, hE);
    } else {
        Serial.println("Enclosure sensor failed, skipping publish");
    }
}

// --- Web page generators ---
String CreateTempDisplayHTML() {
    String ptr = "<!DOCTYPE html><html>\n";
    ptr += "<style>table,th,td{font-size:14px;border:1px solid;border-collapse:collapse;padding:5px;}</style>\n";
    ptr += "<body><h1>CrowPanel Sensor Node</h1>\n";
    ptr += "<p style=\"font-size:24px;\">Outside Temperature: <strong>" + String(lastOutdoorAvgTemp, 1) + " degF</strong></p>\n";
    ptr += "<p style=\"font-size:24px;\">Outside Humidity   : <strong>" + String(lastOutdoorAvgHum, 1) + "%</strong></p>\n";
    ptr += "<p style=\"font-size:24px;\">Timestamp          : <strong>" + buildTimeDateStr() + "</strong></p>\n";
    ptr += "<p style=\"font-size:16px;\">Individual Sensor Data:</p>\n";
    ptr += "<table><tbody>";
    ptr += "<tr><td><strong>ID</strong></td><td><strong>Location</strong></td><td><strong>Temp (degF)</strong></td><td><strong>Humidity (%)</strong></td></tr>\n";
    ptr += "<tr><td>T0</td><td>Outdoor 1</td><td>" + String(lastOutdoor1Temp, 2) + "</td><td>" + String(lastOutdoor1Hum, 2) + "%</td></tr>\n";
    ptr += "<tr><td>T1</td><td>Outdoor 2</td><td>" + String(lastOutdoor2Temp, 2) + "</td><td>" + String(lastOutdoor2Hum, 2) + "%</td></tr>\n";
    ptr += "<tr><td>T2</td><td>Enclosure</td><td>" + String(lastEnclosureTemp, 2) + "</td><td>" + String(lastEnclosureHum, 2) + "%</td></tr>\n";
    ptr += "</tbody></table>\n";
    ptr += "</body></html>\n";
    return ptr;
}

String CreateRootHTML() {
    String ip_str = WiFi.localIP().toString();
    String display_data_url = "http://" + ip_str + "/display_data";

    String ptr = "<!DOCTYPE html><html>\n";
    ptr += "<style>\n";
    ptr += "table, th, td {font-size: 14px;border: 1px solid;border-collapse: collapse;padding: 5px;}\n";
    ptr += "</style>\n";
    ptr += "<body><h1>Sierra - CrowPanel Temp/Humidity Sensor Node</h1>\n";
    ptr += "<p style=\"font-size:20px;\">Sensor display: <a href=" + display_data_url + ">" + display_data_url + "</a></p>\n";
    ptr += "<p style=\"font-size:16px;\">ESP Debug Data:</p>\n";
    ptr += "<p style=\"font-size:16px;\"> System ID: " + String(sys_id) + "</p>\n";
    ptr += "<p style=\"font-size:16px;\"> Version str: " + String(version_str) + "</p>\n";    
    ptr += "<table><tbody>";
    ptr += "<tr><td><strong>Param</strong></td><td><strong>Value</strong></td></tr>\n";
    ptr += "<tr><td>Timestamp</td><td>" + buildTimeDateStr() + "</td></tr>\n";
    ptr += "<tr><td>FreeHeap</td><td>" + String(ESP.getFreeHeap()) + "</td></tr>\n";
    ptr += "<tr><td>MaxFreeBlockSize</td><td>" + String(ESP.getMaxFreeBlockSize()) + "</td></tr>\n";
    ptr += "<tr><td>HeapFragmentation</td><td>" + String(ESP.getHeapFragmentation()) + "</td></tr>\n";
    ptr += "<tr><td>Publish interval (ms)</td><td>" + String(PUBLISH_INTERVAL_MS) + "</td></tr>\n";
    ptr += "</tbody></table>\n";
    ptr += "</body></html>\n";
    return ptr;
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nCrowPanel Sensor Node starting...");

    dhtOutdoor1.setup(DHT_OUTDOOR1_PIN, DHTesp::DHT22);
    dhtOutdoor2.setup(DHT_OUTDOOR2_PIN, DHTesp::DHT22);
    dhtEnclosure.setup(DHT_ENCLOSURE_PIN, DHTesp::DHT22);

    connectWiFi();
    setupOnline();

    wifiClientSecure.setInsecure();
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setBufferSize(256);
    connectMQTT();

    readAndPublish();
    lastPublish = millis();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost, reconnecting...");
        connectWiFi();
    }

    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop();

    // NTP hourly refresh
    if (millis() - lastNTPUpdate >= NTP_INTERVAL_MS) {
        lastNTPUpdate = millis();
        timeClient.update();
    }

    // Publish on interval
    if (millis() - lastPublish >= PUBLISH_INTERVAL_MS) {
        readAndPublish();
        lastPublish = millis();
    }
}
