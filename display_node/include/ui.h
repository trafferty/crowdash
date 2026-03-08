#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// Custom fonts
LV_FONT_DECLARE(ui_font_Big_Number);
LV_FONT_DECLARE(ui_font_Bold_Font);

// Initialize UI
void ui_init(void);

// Time updates
void ui_update_time(const char* time_str, const char* date_str);

// Temperature/humidity updates
void ui_update_outdoor_temp(float temp_f);
void ui_update_enclosure(float temp_f, float humidity);

// Chart (outdoor only, dual-axis)
void ui_chart_add_point(float temp_f, float humidity);

// Garage status
void ui_update_garage_status(const char* status);  // "open" or "closed"

// Legacy (keep for backward compatibility)
void ui_update_current(int sensorIdx, float temp, float humidity);

#ifdef __cplusplus
}
#endif

#endif
