// Application-level UI update functions.
// Bridges main.cpp (MQTT callbacks, time updates) to the
// SquareLine-generated widget objects.

#include "ui.h"
#include "app_ui.h"
#include "audio.h"
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

// Format 24-hour HH/MM into "H:MMam" / "H:MMpm".  buf must be >= 10 bytes.
static void fmt_12h(char* buf, size_t len, int hh, int mm)
{
    const char* ampm = (hh < 12) ? "am" : "pm";
    int h12 = hh % 12;
    if (h12 == 0) h12 = 12;
    snprintf(buf, len, "%d:%02d%s", h12, mm, ampm);
}

// Forward declarations for timer button callbacks
static void timer_btn_start_cb(lv_event_t *e);
static void timer_btn_stop_cb(lv_event_t *e);
static void timer_btn_reset_cb(lv_event_t *e);
static void timer_btn_clear_cb(lv_event_t *e);

// ============================================================
// Gesture navigation
// Screen order (left → right): Dashboard | Timer | Logger
// Swipe LEFT advances right; swipe RIGHT goes back left.
// ============================================================

static void gesture_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;

    lv_indev_t *indev = lv_indev_get_act();
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    lv_obj_t *scr = lv_event_get_target(e);

    if (scr == ui_screendashboard && dir == LV_DIR_LEFT) {
        _ui_screen_change(&ui_screentimer, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, &ui_screentimer_screen_init);
    } else if (scr == ui_screentimer && dir == LV_DIR_RIGHT) {
        _ui_screen_change(&ui_screendashboard, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, &ui_screendashboard_screen_init);
    } else if (scr == ui_screentimer && dir == LV_DIR_LEFT) {
        _ui_screen_change(&ui_screenlogger, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, &ui_screenlogger_screen_init);
    } else if (scr == ui_screenlogger && dir == LV_DIR_RIGHT) {
        _ui_screen_change(&ui_screentimer, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, &ui_screentimer_screen_init);
    } else {
        return;
    }

    lv_indev_wait_release(indev);
}

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

    // Register swipe-to-navigate on all three screens
    lv_obj_add_event_cb(ui_screendashboard, gesture_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(ui_screentimer,     gesture_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(ui_screenlogger,    gesture_event_cb, LV_EVENT_GESTURE, NULL);

    // Wire timer screen buttons
    lv_obj_add_event_cb(ui_btnStartTimer, timer_btn_start_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btnStopTimer,  timer_btn_stop_cb,  LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btnResetTimer, timer_btn_reset_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btnClearTimer, timer_btn_clear_cb, LV_EVENT_CLICKED, NULL);
}

// ============================================================
// Timer state machine
// ============================================================

typedef enum { TIMER_IDLE, TIMER_RUNNING, TIMER_PAUSED, TIMER_ALARM } timer_state_t;

static timer_state_t timer_state    = TIMER_IDLE;
static uint32_t      timer_remaining_ms = 0;
static uint32_t      timer_start_ms    = 0;
static uint32_t      timer_last_tick   = 0;

static void timer_set_display(uint32_t ms)
{
    uint32_t secs = ms / 1000;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02lu:%02lu", secs / 60, secs % 60);
    if (ui_txtTimeRemaining) lv_textarea_set_text(ui_txtTimeRemaining, buf);
}

static void timer_on_expired(void)
{
    timer_state = TIMER_ALARM;
    timer_set_display(0);
    if (ui_barTimer) lv_bar_set_value(ui_barTimer, 0, LV_ANIM_OFF);
    audio_start_alarm();
}

static void timer_btn_start_cb(lv_event_t *e)
{
    (void)e;
    if (timer_state == TIMER_IDLE) {
        const char *txt = ui_txtTimerStartValue
                          ? lv_textarea_get_text(ui_txtTimerStartValue) : "0";
        int val = atoi(txt);
        if (val <= 0) return;
        bool seconds_mode = ui_swtModeMinOrSec
                            && lv_obj_has_state(ui_swtModeMinOrSec, LV_STATE_CHECKED);
        timer_start_ms     = (uint32_t)val * (seconds_mode ? 1000UL : 60000UL);
        timer_remaining_ms = timer_start_ms;
        if (ui_barTimer) lv_bar_set_value(ui_barTimer, 100, LV_ANIM_OFF);
        timer_set_display(timer_remaining_ms);
    }
    if (timer_state == TIMER_IDLE || timer_state == TIMER_PAUSED) {
        timer_state     = TIMER_RUNNING;
        timer_last_tick = millis();
    }
}

static void timer_btn_stop_cb(lv_event_t *e)
{
    (void)e;
    if (timer_state == TIMER_RUNNING) timer_state = TIMER_PAUSED;
}

static void timer_btn_reset_cb(lv_event_t *e)
{
    (void)e;
    audio_stop_alarm();
    timer_state        = TIMER_IDLE;
    timer_remaining_ms = 0;
    timer_set_display(0);
    if (ui_barTimer) lv_bar_set_value(ui_barTimer, 100, LV_ANIM_OFF);
}

static void timer_btn_clear_cb(lv_event_t *e)
{
    (void)e;
    if (timer_state == TIMER_IDLE && ui_txtTimerStartValue)
        lv_textarea_set_text(ui_txtTimerStartValue, "");
}

void ui_timer_tick(uint32_t now_ms)
{
    if (timer_state != TIMER_RUNNING) return;

    uint32_t elapsed = now_ms - timer_last_tick;
    timer_last_tick  = now_ms;

    if (elapsed >= timer_remaining_ms) {
        timer_remaining_ms = 0;
        timer_on_expired();
        return;
    }

    timer_remaining_ms -= elapsed;
    timer_set_display(timer_remaining_ms);

    if (ui_barTimer && timer_start_ms > 0) {
        int32_t pct = (int32_t)((uint64_t)timer_remaining_ms * 100 / timer_start_ms);
        lv_bar_set_value(ui_barTimer, pct, LV_ANIM_OFF);
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
    // ts format: "2026-04-12T14:39:15"
    const char* t_pos = strchr(ts, 'T');
    if (!t_pos) return;
    int hh = 0, mm = 0;
    if (sscanf(t_pos + 1, "%d:%d", &hh, &mm) != 2) return;
    char buf[10];
    fmt_12h(buf, sizeof(buf), hh, mm);
    lv_label_set_text(ui_lblGarageTime, buf);
}

// ============================================================
// Sensor timestamp
// ============================================================
void ui_update_sensor_timestamp(const char* ts)
{
    // ts format: "2025-11-15T17:18:25"
    const char* t_pos = strchr(ts, 'T');
    if (!t_pos) return;

    int hh = 0, mm = 0;
    if (sscanf(t_pos + 1, "%d:%d", &hh, &mm) != 2) return;

    char ts_short[10];
    fmt_12h(ts_short, sizeof(ts_short), hh, mm);

    if (ui_lblEvent1)       lv_label_set_text(ui_lblEvent1,       "Last Update");
    if (ui_lblEventStatus1) lv_label_set_text(ui_lblEventStatus1, ts_short);

    char buf[28];
    if (ui_lblTemp) {
        snprintf(buf, sizeof(buf), "Temp - %s", ts_short);
        lv_label_set_text(ui_lblTemp, buf);
    }
    if (ui_lblHumidity) {
        snprintf(buf, sizeof(buf), "Humidity - %s", ts_short);
        lv_label_set_text(ui_lblHumidity, buf);
    }
}

// ============================================================
// Legacy stub
// ============================================================
void ui_update_current(int sensorIdx, float temp, float humidity)
{
    (void)sensorIdx; (void)temp; (void)humidity;
}
