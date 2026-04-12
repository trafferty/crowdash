#ifndef APP_UI_H
#define APP_UI_H

#include "ui.h"

#ifdef __cplusplus
extern "C" {
#endif

// Timestamped log: writes to Serial and appends to the logger screen.
// Safe to call before ui_init() — output goes to Serial only until the
// logger widget exists.
void ui_log(const char* fmt, ...);

// Call once after ui_init() to set up chart series
void ui_app_init(void);

// Time/date display (called every second from main loop)
void ui_update_time(const char* time_str, const char* date_str);

// Outdoor sensor (MQTT: crowpanel/outdoor)
void ui_update_outdoor_temp(float temp_f);
void ui_chart_add_point(float temp_f, float humidity);

// Enclosure sensor (MQTT: crowpanel/enclosure)
void ui_update_enclosure(float temp_f, float humidity);

// Garage door (MQTT: crowpanel/garage)
// Payload: {"event":"open","ts":"..."} or {"event":"closed","ts":"..."}
void ui_update_garage_status(const char* status);  // "open" or "closed"
void ui_update_garage_time(const char* ts);        // ISO 8601 timestamp

// Sensor timestamp (MQTT: crowpanel/outdoor "ts" field, ISO 8601)
void ui_update_sensor_timestamp(const char* ts);

// Legacy stub kept for backward compatibility with mqttCallback
void ui_update_current(int sensorIdx, float temp, float humidity);

#ifdef __cplusplus
}
#endif

#endif // APP_UI_H
