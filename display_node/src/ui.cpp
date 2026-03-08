#include "ui.h"
#include <stdio.h>

// Color scheme
#define COLOR_BG           lv_color_hex(0x1A1A50)  // Deep blue
#define COLOR_PANEL        lv_color_hex(0x16213E)  // Dark panel
#define COLOR_GARAGE_BG    lv_color_hex(0x3A6EA5)  // Blue garage
#define COLOR_STATUS_OPEN  lv_color_hex(0xFF6B6B)  // Red (open)
#define COLOR_STATUS_CLOSED lv_color_hex(0x6FCF97) // Green (closed)
#define COLOR_OUTDOOR_TEMP lv_color_hex(0xFF6B6B)  // Red
#define COLOR_OUTDOOR_HUM  lv_color_hex(0x4ECDC4)  // Teal
#define COLOR_ENCL         lv_color_hex(0x4ECDC4)  // Teal
#define COLOR_TEXT_PRIMARY lv_color_hex(0xFFFFFF)  // White
#define COLOR_TEXT_DIM     lv_color_hex(0x888888)  // Gray
#define COLOR_ICON_PLACEHOLDER lv_color_hex(0xFFD700) // Gold

#define CHART_POINTS 72  // 6 hours at 5-min intervals

// UI objects
static lv_obj_t *screen;

// Clock panel
static lv_obj_t *panel_clock;
static lv_obj_t *lbl_time;
static lv_obj_t *lbl_date;
static lv_obj_t *lbl_outdoor_temp;
static lv_obj_t *lbl_enclosure_temp;
static lv_obj_t *rect_weather_icon;

// Calendar sidebar
static lv_obj_t *panel_calendar;
static lv_obj_t *lbl_calendar_items[10];

// Forecast panels
static lv_obj_t *panel_forecast[5];
static lv_obj_t *lbl_forecast_day[5];
static lv_obj_t *lbl_forecast_temp[5];
static lv_obj_t *rect_forecast_icon[5];

// Garage panel
static lv_obj_t *panel_garage;
static lv_obj_t *lbl_garage_status;

// Combined chart (outdoor temp + humidity)
static lv_obj_t *chart;
static lv_chart_series_t *ser_temp;
static lv_chart_series_t *ser_hum;

// Helper: Clock panel (400x170)
static void create_clock_panel(lv_obj_t *parent) {
    panel_clock = lv_obj_create(parent);
    lv_obj_set_pos(panel_clock, 0, 0);
    lv_obj_set_size(panel_clock, 400, 170);
    lv_obj_set_style_bg_color(panel_clock, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(panel_clock, 0, 0);
    lv_obj_set_style_radius(panel_clock, 8, 0);
    lv_obj_clear_flag(panel_clock, LV_OBJ_FLAG_SCROLLABLE);

    // Time (HH:MM) - Big Number font
    lbl_time = lv_label_create(panel_clock);
    lv_label_set_text(lbl_time, "12:00");
    lv_obj_set_style_text_color(lbl_time, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl_time, &ui_font_Big_Number, 0);
    lv_obj_set_pos(lbl_time, 15, 5);

    // Date
    lbl_date = lv_label_create(panel_clock);
    lv_label_set_text(lbl_date, "Saturday, Mar 7");
    lv_obj_set_style_text_color(lbl_date, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_date, 15, 85);

    // Outdoor temp (large)
    lbl_outdoor_temp = lv_label_create(panel_clock);
    lv_label_set_text(lbl_outdoor_temp, "-- F");
    lv_obj_set_style_text_color(lbl_outdoor_temp, COLOR_OUTDOOR_TEMP, 0);
    lv_obj_set_style_text_font(lbl_outdoor_temp, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(lbl_outdoor_temp, 210, 15);

    // Weather icon placeholder
    rect_weather_icon = lv_obj_create(panel_clock);
    lv_obj_set_pos(rect_weather_icon, 315, 10);
    lv_obj_set_size(rect_weather_icon, 70, 70);
    lv_obj_set_style_bg_color(rect_weather_icon, COLOR_ICON_PLACEHOLDER, 0);
    lv_obj_set_style_border_width(rect_weather_icon, 0, 0);
    lv_obj_set_style_radius(rect_weather_icon, 35, 0);

    // Enclosure reading
    lbl_enclosure_temp = lv_label_create(panel_clock);
    lv_label_set_text(lbl_enclosure_temp, "Enclosure: -- F, --%");
    lv_obj_set_style_text_color(lbl_enclosure_temp, COLOR_ENCL, 0);
    lv_obj_set_style_text_font(lbl_enclosure_temp, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_enclosure_temp, 210, 95);
}

// Helper: Calendar sidebar (150x90)
static void create_calendar_sidebar(lv_obj_t *parent) {
    panel_calendar = lv_obj_create(parent);
    lv_obj_set_pos(panel_calendar, 0, 170);
    lv_obj_set_size(panel_calendar, 150, 90);
    lv_obj_set_style_bg_color(panel_calendar, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(panel_calendar, 0, 0);
    lv_obj_set_style_radius(panel_calendar, 8, 0);
    lv_obj_clear_flag(panel_calendar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel_calendar);
    lv_label_set_text(title, "Events");
    lv_obj_set_style_text_color(title, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(title, 10, 5);

    for (int i = 0; i < 10; i++) {
        lbl_calendar_items[i] = lv_label_create(panel_calendar);
        lv_label_set_text_fmt(lbl_calendar_items[i], "List line %d", i + 1);
        lv_obj_set_style_text_color(lbl_calendar_items[i], COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl_calendar_items[i], &lv_font_montserrat_10, 0);
        lv_obj_set_pos(lbl_calendar_items[i], 10, 22 + (i * 7));
    }
}

// Helper: Forecast panels (5 x 125px wide)
static void create_forecast_panels(lv_obj_t *parent) {
    const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday"};
    const lv_color_t colors[] = {
        lv_color_hex(0xFFD700), lv_color_hex(0xC0C0C0), lv_color_hex(0x87CEEB),
        lv_color_hex(0xB0C4DE), lv_color_hex(0xFFA500)
    };

    for (int i = 0; i < 5; i++) {
        panel_forecast[i] = lv_obj_create(parent);
        lv_obj_set_pos(panel_forecast[i], 155 + (i * 130), 170);
        lv_obj_set_size(panel_forecast[i], 125, 85);
        lv_obj_set_style_bg_color(panel_forecast[i], COLOR_PANEL, 0);
        lv_obj_set_style_border_width(panel_forecast[i], 0, 0);
        lv_obj_set_style_radius(panel_forecast[i], 8, 0);
        lv_obj_clear_flag(panel_forecast[i], LV_OBJ_FLAG_SCROLLABLE);

        // Icon
        rect_forecast_icon[i] = lv_obj_create(panel_forecast[i]);
        lv_obj_set_pos(rect_forecast_icon[i], 35, 5);
        lv_obj_set_size(rect_forecast_icon[i], 50, 50);
        lv_obj_set_style_bg_color(rect_forecast_icon[i], colors[i], 0);
        lv_obj_set_style_border_width(rect_forecast_icon[i], 0, 0);
        lv_obj_set_style_radius(rect_forecast_icon[i], 25, 0);

        // Day
        lbl_forecast_day[i] = lv_label_create(panel_forecast[i]);
        lv_label_set_text(lbl_forecast_day[i], days[i]);
        lv_obj_set_style_text_color(lbl_forecast_day[i], COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(lbl_forecast_day[i], &lv_font_montserrat_10, 0);
        lv_obj_align(lbl_forecast_day[i], LV_ALIGN_BOTTOM_MID, 0, -15);

        // Temp
        lbl_forecast_temp[i] = lv_label_create(panel_forecast[i]);
        lv_label_set_text(lbl_forecast_temp[i], "H:82 L:63");
        lv_obj_set_style_text_color(lbl_forecast_temp[i], COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl_forecast_temp[i], &lv_font_montserrat_10, 0);
        lv_obj_align(lbl_forecast_temp[i], LV_ALIGN_BOTTOM_MID, 0, -3);
    }
}

// Helper: Combined chart (795x110, dual-axis)
static void create_combined_chart(lv_obj_t *parent) {
    // Title
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Outdoor: Temperature & Humidity (6 hours)");
    lv_obj_set_style_text_color(title, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(title, 10, 265);

    // Chart
    chart = lv_chart_create(parent);
    lv_obj_set_pos(chart, 5, 280);
    lv_obj_set_size(chart, 790, 85);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, CHART_POINTS);
    lv_chart_set_div_line_count(chart, 4, 0);

    lv_obj_set_style_bg_color(chart, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(chart, 0, 0);
    lv_obj_set_style_radius(chart, 8, 0);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);

    // Dual Y-axis: Temp (0-120°F), Humidity (0-100%)
    // Use secondary axis for humidity
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1200);   // Temp x10 (0-120°F)
    lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, 0, 1000); // Humidity x10 (0-100%)

    // Series: temp on primary axis, humidity on secondary
    ser_temp = lv_chart_add_series(chart, COLOR_OUTDOOR_TEMP, LV_CHART_AXIS_PRIMARY_Y);
    ser_hum = lv_chart_add_series(chart, COLOR_OUTDOOR_HUM, LV_CHART_AXIS_SECONDARY_Y);

    // Initialize with no data
    for (int i = 0; i < CHART_POINTS; i++) {
        lv_chart_set_next_value(chart, ser_temp, LV_CHART_POINT_NONE);
        lv_chart_set_next_value(chart, ser_hum, LV_CHART_POINT_NONE);
    }
}

// Helper: Garage panel (800x60)
static void create_garage_panel(lv_obj_t *parent) {
    panel_garage = lv_obj_create(parent);
    lv_obj_set_pos(panel_garage, 0, 370);
    lv_obj_set_size(panel_garage, 800, 60);
    lv_obj_set_style_bg_color(panel_garage, COLOR_GARAGE_BG, 0);
    lv_obj_set_style_border_width(panel_garage, 0, 0);
    lv_obj_set_style_radius(panel_garage, 8, 0);
    lv_obj_clear_flag(panel_garage, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(panel_garage);
    lv_label_set_text(label, "Garage Door:");
    lv_obj_set_style_text_color(label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(label, 260, 18);

    lbl_garage_status = lv_label_create(panel_garage);
    lv_label_set_text(lbl_garage_status, "  Unknown  ");
    lv_obj_set_style_text_color(lbl_garage_status, lv_color_black(), 0);
    lv_obj_set_style_text_font(lbl_garage_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(lbl_garage_status, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_bg_opa(lbl_garage_status, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(lbl_garage_status, 15, 0);
    lv_obj_set_style_pad_all(lbl_garage_status, 8, 0);
    lv_obj_set_pos(lbl_garage_status, 410, 15);
}

// Main init function
void ui_init(void) {
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    create_clock_panel(screen);
    create_calendar_sidebar(screen);
    create_forecast_panels(screen);
    create_combined_chart(screen);
    create_garage_panel(screen);

    lv_disp_load_scr(screen);
}

// API implementation
void ui_update_time(const char* time_str, const char* date_str) {
    lv_label_set_text(lbl_time, time_str);
    lv_label_set_text(lbl_date, date_str);
}

void ui_update_outdoor_temp(float temp_f) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f F", temp_f);
    lv_label_set_text(lbl_outdoor_temp, buf);
}

void ui_update_enclosure(float temp_f, float humidity) {
    char buf[40];
    snprintf(buf, sizeof(buf), "Enclosure: %.0f F, %.0f%%", temp_f, humidity);
    lv_label_set_text(lbl_enclosure_temp, buf);
}

void ui_chart_add_point(float temp_f, float humidity) {
    lv_chart_set_next_value(chart, ser_temp, (lv_coord_t)(temp_f * 10));
    lv_chart_set_next_value(chart, ser_hum, (lv_coord_t)(humidity * 10));
    lv_chart_refresh(chart);
}

void ui_update_garage_status(const char* status) {
    if (strcmp(status, "open") == 0) {
        lv_label_set_text(lbl_garage_status, "   Open   ");
        lv_obj_set_style_bg_color(lbl_garage_status, COLOR_STATUS_OPEN, 0);
    } else if (strcmp(status, "closed") == 0) {
        lv_label_set_text(lbl_garage_status, "  Closed  ");
        lv_obj_set_style_bg_color(lbl_garage_status, COLOR_STATUS_CLOSED, 0);
    } else {
        lv_label_set_text(lbl_garage_status, " Unknown ");
        lv_obj_set_style_bg_color(lbl_garage_status, COLOR_TEXT_DIM, 0);
    }
}

// Legacy compatibility
void ui_update_current(int sensorIdx, float temp, float humidity) {
    // Keep empty for backward compatibility with MQTT callback
}
