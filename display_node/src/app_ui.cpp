// Application-level UI update functions.
// Bridges main.cpp (MQTT callbacks, time updates) to the
// SquareLine-generated widget objects.

#include "ui.h"
#include "app_ui.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <Arduino.h>

// ============================================================
// Logger
// ============================================================
#define LOG_MAX_CHARS 4000

void ui_log(const char* fmt, ...)
{
    char msg[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    // Build timestamped line
    char line[300];
    struct tm ti;
    if (getLocalTime(&ti, 0)) {
        snprintf(line, sizeof(line), "[%02d:%02d:%02d] %s\n",
                 ti.tm_hour, ti.tm_min, ti.tm_sec, msg);
    } else {
        snprintf(line, sizeof(line), "[--:--:--] %s\n", msg);
    }

    Serial.print(line);

    if (!ui_txtLogger) return;

    // Roll over when the textarea gets too long
    if (strlen(lv_textarea_get_text(ui_txtLogger)) > LOG_MAX_CHARS) {
        lv_textarea_set_text(ui_txtLogger, "[...log cleared...]\n");
    }

    lv_textarea_set_cursor_pos(ui_txtLogger, LV_TEXTAREA_CURSOR_LAST);
    lv_textarea_add_text(ui_txtLogger, line);
}

// ============================================================
// Chart management
// ============================================================
#define CHART_POINTS 20

static lv_coord_t        temp_data[CHART_POINTS];
static lv_coord_t        hum_data[CHART_POINTS];
static lv_chart_series_t *ser_temp = NULL;
static lv_chart_series_t *ser_hum  = NULL;
static float             temp_min_seen =  999.0f;
static float             temp_max_seen = -999.0f;

static lv_obj_t *ui_lblGarageTime = NULL;

void ui_app_init(void)
{
    // Initialize data arrays to "no data"
    for (int i = 0; i < CHART_POINTS; i++) {
        temp_data[i] = LV_CHART_POINT_NONE;
        hum_data[i]  = LV_CHART_POINT_NONE;
    }

    // Retrieve the series created by SquareLine in screendashboard init
    ser_temp = lv_chart_get_series_next(ui_chtTemp,     NULL);
    ser_hum  = lv_chart_get_series_next(ui_chtHumidity, NULL);

    // Replace SquareLine's dummy arrays with our managed arrays
    if (ser_temp) lv_chart_set_ext_y_array(ui_chtTemp,     ser_temp, temp_data);
    if (ser_hum)  lv_chart_set_ext_y_array(ui_chtHumidity, ser_hum,  hum_data);

    lv_chart_refresh(ui_chtTemp);
    lv_chart_refresh(ui_chtHumidity);

    // Expand garage panel and add timestamp label at bottom
    if (ui_pnlGarageDoorStatus) {
        lv_obj_set_height(ui_pnlGarageDoorStatus, 100);
        ui_lblGarageTime = lv_label_create(ui_pnlGarageDoorStatus);
        lv_obj_set_width(ui_lblGarageTime, LV_SIZE_CONTENT);
        lv_obj_set_height(ui_lblGarageTime, LV_SIZE_CONTENT);
        lv_obj_set_align(ui_lblGarageTime, LV_ALIGN_CENTER);
        lv_obj_set_y(ui_lblGarageTime, 40);
        lv_label_set_text(ui_lblGarageTime, "--:--");
        lv_obj_set_style_text_font(ui_lblGarageTime, &lv_font_montserrat_14,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_lblGarageTime, lv_color_hex(0xAAAAAA),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

// ============================================================
// Time / date
// ============================================================
void ui_update_time(const char* time_str, const char* date_str)
{
    if (ui_lblTimeOfDay) lv_label_set_text(ui_lblTimeOfDay, time_str);
    if (ui_lblDate)      lv_label_set_text(ui_lblDate,      date_str);
}

// ============================================================
// Outdoor sensor
// ============================================================
void ui_update_outdoor_temp(float temp_f)
{
    if (!ui_lblCurrentTemp) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f F", temp_f);
    lv_label_set_text(ui_lblCurrentTemp, buf);
}

void ui_chart_add_point(float temp_f, float humidity)
{
    if (!ser_temp || !ser_hum) return;

    // Keep dynamic Y-axis range for temp chart
    if (temp_f < temp_min_seen) temp_min_seen = temp_f;
    if (temp_f > temp_max_seen) temp_max_seen = temp_f;
    lv_chart_set_range(ui_chtTemp, LV_CHART_AXIS_PRIMARY_Y,
                       (lv_coord_t)(temp_min_seen - 5),
                       (lv_coord_t)(temp_max_seen + 5));

    lv_chart_set_next_value(ui_chtTemp,     ser_temp, (lv_coord_t)temp_f);
    lv_chart_set_next_value(ui_chtHumidity, ser_hum,  (lv_coord_t)humidity);
    lv_chart_refresh(ui_chtTemp);
    lv_chart_refresh(ui_chtHumidity);

    // Also update the current humidity label
    if (ui_lblCurrentHumidity) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%.0f%%", humidity);
        lv_label_set_text(ui_lblCurrentHumidity, buf);
    }
}

// ============================================================
// Enclosure sensor
// ============================================================
void ui_update_enclosure(float temp_f, float humidity)
{
    if (ui_lblEnclosureTemp) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%.0f F", temp_f);
        lv_label_set_text(ui_lblEnclosureTemp, buf);
    }
    if (ui_lblEnclosureHumidity) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0f%%", humidity);
        lv_label_set_text(ui_lblEnclosureHumidity, buf);
    }
}

// ============================================================
// Garage door
// ============================================================
void ui_update_garage_status(const char* status)
{
    if (!ui_lblGarageDoorStatus) return;
    if (strcmp(status, "open") == 0) {
        lv_label_set_text(ui_lblGarageDoorStatus, "Open");
        lv_obj_set_style_text_color(ui_lblGarageDoorStatus,
                                    lv_color_hex(0xFF6B6B),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        if (ui_pnlGarageDoorStatus)
            lv_obj_set_style_bg_color(ui_pnlGarageDoorStatus,
                                      lv_color_hex(0x8B0000),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (strcmp(status, "closed") == 0) {
        lv_label_set_text(ui_lblGarageDoorStatus, "Closed");
        lv_obj_set_style_text_color(ui_lblGarageDoorStatus,
                                    lv_color_hex(0x6FCF97),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        if (ui_pnlGarageDoorStatus)
            lv_obj_set_style_bg_color(ui_pnlGarageDoorStatus,
                                      lv_color_hex(0x1A5C2A),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_label_set_text(ui_lblGarageDoorStatus, "Unknown");
        lv_obj_set_style_text_color(ui_lblGarageDoorStatus,
                                    lv_color_hex(0x888888),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        if (ui_pnlGarageDoorStatus)
            lv_obj_set_style_bg_color(ui_pnlGarageDoorStatus,
                                      lv_color_hex(0x1D254A),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

// ============================================================
// Garage timestamp
// ============================================================
void ui_update_garage_time(const char* ts)
{
    if (!ui_lblGarageTime) return;
    // ts format: "2026-04-12T14:39:15" — show "HH:MM" after the 'T'
    const char* t_pos = strchr(ts, 'T');
    if (!t_pos) return;
    char hhmm[6];
    strncpy(hhmm, t_pos + 1, 5);
    hhmm[5] = '\0';
    lv_label_set_text(ui_lblGarageTime, hhmm);
}

// ============================================================
// Sensor timestamp
// ============================================================
void ui_update_sensor_timestamp(const char* ts)
{
    // ts format: "2025-11-15T17:18:25" — extract HH:MM after the 'T'
    const char* t_pos = strchr(ts, 'T');
    if (!t_pos) return;
    char hhmm[6];
    strncpy(hhmm, t_pos + 1, 5);
    hhmm[5] = '\0';

    if (ui_lblEvent1)       lv_label_set_text(ui_lblEvent1,       "Last Update");
    if (ui_lblEventStatus1) lv_label_set_text(ui_lblEventStatus1, hhmm);
}

// ============================================================
// Legacy stub
// ============================================================
void ui_update_current(int sensorIdx, float temp, float humidity)
{
    (void)sensorIdx; (void)temp; (void)humidity;
}
