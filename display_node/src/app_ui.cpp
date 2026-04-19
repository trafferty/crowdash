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
static void timer_input_changed_cb(lv_event_t *e);

// Forward declarations for schedule callbacks
static void schedule_save_cb(lv_event_t *e);
static void schedule_sleep_ta_cb(lv_event_t *e);
static void schedule_wake_ta_cb(lv_event_t *e);

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
        _ui_screen_change(&ui_screentimer,    LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, &ui_screentimer_screen_init);
    } else if (scr == ui_screenlogger && dir == LV_DIR_LEFT) {
        _ui_screen_change(&ui_screenschedule, LV_SCR_LOAD_ANIM_MOVE_LEFT,  200, 0, &ui_screenschedule_screen_init);
    } else if (scr == ui_screenschedule && dir == LV_DIR_RIGHT) {
        _ui_screen_change(&ui_screenlogger,   LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, &ui_screenlogger_screen_init);
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

    // Dim time/date labels until NTP syncs
    lv_color_t pre_ntp = lv_color_hex(0x555555);
    if (ui_lblTimeOfDay) lv_obj_set_style_text_color(ui_lblTimeOfDay, pre_ntp, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (ui_lblDate)      lv_obj_set_style_text_color(ui_lblDate,      pre_ntp, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Register swipe-to-navigate on all three screens
    lv_obj_add_event_cb(ui_screendashboard, gesture_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(ui_screentimer,     gesture_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(ui_screenlogger,    gesture_event_cb, LV_EVENT_GESTURE, NULL);

    // Wire timer screen buttons
    lv_obj_add_event_cb(ui_btnStartTimer, timer_btn_start_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btnStopTimer,  timer_btn_stop_cb,  LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btnResetTimer, timer_btn_reset_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btnClearTimer, timer_btn_clear_cb, LV_EVENT_CLICKED, NULL);

    // Live-preview timer display while entering a value in IDLE state
    lv_obj_add_event_cb(ui_txtTimerStartValue, timer_input_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_swtModeMinOrSec,    timer_input_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Schedule screen
    lv_obj_add_event_cb(ui_screenschedule,  gesture_event_cb,     LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(ui_txtSleepTime,    schedule_sleep_ta_cb, LV_EVENT_CLICKED,  NULL);
    lv_obj_add_event_cb(ui_txtWakeTime,     schedule_wake_ta_cb,  LV_EVENT_CLICKED,  NULL);
    lv_obj_add_event_cb(ui_btnSaveSchedule, schedule_save_cb,     LV_EVENT_CLICKED,  NULL);
}

// ============================================================
// Timer state machine
// ============================================================

typedef enum { TIMER_IDLE, TIMER_RUNNING, TIMER_PAUSED, TIMER_ALARM } timer_state_t;

static timer_state_t timer_state    = TIMER_IDLE;
static uint32_t      timer_remaining_ms = 0;
static uint32_t      timer_start_ms    = 0;
static uint32_t      timer_last_tick   = 0;

static void keypad_set_enabled(bool en)
{
    if (ui_kbNumberPad) {
        if (en) lv_obj_clear_state(ui_kbNumberPad, LV_STATE_DISABLED);
        else    lv_obj_add_state(ui_kbNumberPad,   LV_STATE_DISABLED);
    }
    if (ui_swtModeMinOrSec) {
        if (en) lv_obj_clear_state(ui_swtModeMinOrSec, LV_STATE_DISABLED);
        else    lv_obj_add_state(ui_swtModeMinOrSec,   LV_STATE_DISABLED);
    }
    if (ui_txtTimerStartValue) {
        if (en) lv_obj_clear_state(ui_txtTimerStartValue, LV_STATE_DISABLED);
        else    lv_obj_add_state(ui_txtTimerStartValue,   LV_STATE_DISABLED);
    }
}

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
        keypad_set_enabled(false);
    }
}

static void timer_btn_stop_cb(lv_event_t *e)
{
    (void)e;
    if (timer_state == TIMER_RUNNING) timer_state = TIMER_PAUSED;
    if (timer_state == TIMER_ALARM)  { audio_stop_alarm(); timer_state = TIMER_IDLE; keypad_set_enabled(true); }
}

static void timer_btn_reset_cb(lv_event_t *e)
{
    (void)e;
    audio_stop_alarm();
    timer_state        = TIMER_IDLE;
    timer_remaining_ms = 0;
    timer_set_display(0);
    if (ui_barTimer) lv_bar_set_value(ui_barTimer, 100, LV_ANIM_OFF);
    keypad_set_enabled(true);
}

static void timer_btn_clear_cb(lv_event_t *e)
{
    (void)e;
    if (timer_state == TIMER_ALARM)  { audio_stop_alarm(); timer_state = TIMER_IDLE; keypad_set_enabled(true); }
    if (timer_state == TIMER_IDLE && ui_txtTimerStartValue)
        lv_textarea_set_text(ui_txtTimerStartValue, "");
}

static void timer_input_changed_cb(lv_event_t *e)
{
    (void)e;
    if (timer_state != TIMER_IDLE) return;
    const char *txt = ui_txtTimerStartValue
                      ? lv_textarea_get_text(ui_txtTimerStartValue) : "0";
    int val = atoi(txt);
    bool seconds_mode = ui_swtModeMinOrSec
                        && lv_obj_has_state(ui_swtModeMinOrSec, LV_STATE_CHECKED);
    timer_set_display((uint32_t)val * (seconds_mode ? 1000UL : 60000UL));
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
// Schedule state machine
// ============================================================

static int      sched_sleep_h       = 22;   // default 10:00 PM
static int      sched_sleep_m       = 0;
static int      sched_wake_h        = 7;    // default 7:00 AM
static int      sched_wake_m        = 0;
static bool     schedule_enabled    = false;
static bool     display_is_on       = true;
static uint32_t touch_wake_until_ms = 0;    // non-zero while touch-woken
#define TOUCH_WAKE_MS 60000UL

static void schedule_sleep_ta_cb(lv_event_t *e)
{
    (void)e;
    if (ui_kbSchedule && ui_txtSleepTime)
        lv_keyboard_set_textarea(ui_kbSchedule, ui_txtSleepTime);
}

static void schedule_wake_ta_cb(lv_event_t *e)
{
    (void)e;
    if (ui_kbSchedule && ui_txtWakeTime)
        lv_keyboard_set_textarea(ui_kbSchedule, ui_txtWakeTime);
}

// Parses a 4-digit HHMM textarea + AM/PM switch into 24-hour h/m.
// Hour range: 1–12; minute range: 0–59. Returns false on bad input.
static bool parse_hhmm(lv_obj_t *ta, lv_obj_t *ampm_swt, int *out_h, int *out_m)
{
    if (!ta) return false;
    const char *txt = lv_textarea_get_text(ta);
    if (!txt || strlen(txt) != 4) return false;
    for (int i = 0; i < 4; i++) {
        if (txt[i] < '0' || txt[i] > '9') return false;
    }
    int hh = (txt[0] - '0') * 10 + (txt[1] - '0');
    int mm = (txt[2] - '0') * 10 + (txt[3] - '0');
    if (hh < 1 || hh > 12) return false;
    if (mm > 59) return false;
    bool is_pm = ampm_swt && lv_obj_has_state(ampm_swt, LV_STATE_CHECKED);
    if (hh == 12) hh = 0;   // 12:xx → 0 before applying PM offset
    if (is_pm)    hh += 12; // 0+12=12 (noon), 1..11+12=13..23
    *out_h = hh;
    *out_m = mm;
    return true;
}

static void schedule_save_cb(lv_event_t *e)
{
    (void)e;
    int sh, sm, wh, wm;
    bool sleep_ok = parse_hhmm(ui_txtSleepTime, ui_swtSleepAmPm, &sh, &sm);
    bool wake_ok  = parse_hhmm(ui_txtWakeTime,  ui_swtWakeAmPm,  &wh, &wm);
    if (!sleep_ok || !wake_ok) {
        ui_log("Schedule: invalid time — enter 4 digits (HHMM), hour 01-12");
        return;
    }
    sched_sleep_h    = sh;
    sched_sleep_m    = sm;
    sched_wake_h     = wh;
    sched_wake_m     = wm;
    schedule_enabled = ui_swtScheduleEnable
                       && lv_obj_has_state(ui_swtScheduleEnable, LV_STATE_CHECKED);
    ui_log("Schedule: sleep=%02d:%02d  wake=%02d:%02d  enabled=%s",
           sh, sm, wh, wm, schedule_enabled ? "yes" : "no");
}

bool ui_sleep_intercept_touch(void)
{
    if (display_is_on) return false;
    ledcWrite(1, 255);
    display_is_on       = true;
    touch_wake_until_ms = millis() + TOUCH_WAKE_MS;
    ui_log("Schedule: touch woke display (60s)");
    return true;
}

// Shared helper — evaluate schedule against current time and drive backlight.
static void schedule_apply(int cur_hour, int cur_min)
{
    int now_min   = cur_hour * 60 + cur_min;
    int sleep_min = sched_sleep_h * 60 + sched_sleep_m;
    int wake_min  = sched_wake_h  * 60 + sched_wake_m;

    bool in_sleep;
    if (sleep_min < wake_min)
        in_sleep = (now_min >= sleep_min) && (now_min < wake_min);
    else
        in_sleep = (now_min >= sleep_min) || (now_min < wake_min);

    if (in_sleep && display_is_on) {
        ledcWrite(1, 0);
        display_is_on = false;
        ui_log("Schedule: display off (%02d:%02d)", cur_hour, cur_min);
    } else if (!in_sleep && !display_is_on) {
        ledcWrite(1, 255);
        display_is_on = true;
        ui_log("Schedule: display on (%02d:%02d)", cur_hour, cur_min);
    }
}

void ui_schedule_check(uint32_t now_ms)
{
    if (!schedule_enabled || touch_wake_until_ms == 0) return;
    if (now_ms < touch_wake_until_ms) return;
    touch_wake_until_ms = 0;
    struct tm t;
    if (getLocalTime(&t, 0)) schedule_apply(t.tm_hour, t.tm_min);
}

void ui_schedule_tick(struct tm *t)
{
    if (!t || !schedule_enabled) return;
    if (touch_wake_until_ms != 0) return;  // touch-wake in progress; check handled by ui_schedule_check
    schedule_apply(t->tm_hour, t->tm_min);
}

// ============================================================
// Time / date
// ============================================================
void ui_update_time(const char* time_str, const char* date_str)
{
    static bool ntp_color_set = false;
    if (!ntp_color_set) {
        lv_color_t white = lv_color_hex(0xFFFFFF);
        if (ui_lblTimeOfDay) lv_obj_set_style_text_color(ui_lblTimeOfDay, white, LV_PART_MAIN | LV_STATE_DEFAULT);
        if (ui_lblDate)      lv_obj_set_style_text_color(ui_lblDate,      white, LV_PART_MAIN | LV_STATE_DEFAULT);
        ntp_color_set = true;
    }
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
