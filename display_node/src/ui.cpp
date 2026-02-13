#include "ui.h"
#include <stdio.h>

// ============================================================
// Color scheme
// ============================================================
#define COLOR_BG        lv_color_hex(0x1A1A2E)
#define COLOR_PANEL     lv_color_hex(0x16213E)
#define COLOR_S1_LINE   lv_color_hex(0xFF6B6B)   // Red - Outdoor
#define COLOR_S2_LINE   lv_color_hex(0x4ECDC4)   // Teal - Enclosure
#define COLOR_TEXT_DIM   lv_color_hex(0x888888)
#define COLOR_TEXT_LABEL lv_color_hex(0xAAAAAA)

// ============================================================
// Chart config
// ============================================================
#define CHART_POINTS 72  // 6 hours at 5-min intervals

// ============================================================
// UI objects
// ============================================================
static lv_obj_t *screen;
static lv_obj_t *lbl_status;

// Current value labels: [0]=outdoor, [1]=enclosure
static lv_obj_t *lbl_temp[2];
static lv_obj_t *lbl_hum[2];

// Charts
static lv_obj_t *chart_temp;
static lv_obj_t *chart_hum;

// Chart series: [0]=outdoor, [1]=enclosure
static lv_chart_series_t *ser_temp[2];
static lv_chart_series_t *ser_hum[2];

// ============================================================
// Helper: create a sensor info panel
// ============================================================
static void create_sensor_panel(lv_obj_t *parent, int idx, const char *name,
                                 lv_color_t accent, int x_pos) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x_pos, 30);
    lv_obj_set_size(panel, 392, 115);
    lv_obj_set_style_bg_color(panel, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // Sensor name
    lv_obj_t *lbl_name = lv_label_create(panel);
    lv_label_set_text(lbl_name, name);
    lv_obj_set_style_text_color(lbl_name, accent, 0);
    lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(lbl_name, 15, 8);

    // Temperature value
    lbl_temp[idx] = lv_label_create(panel);
    lv_label_set_text(lbl_temp[idx], "--.- C");
    lv_obj_set_style_text_color(lbl_temp[idx], lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_temp[idx], &lv_font_montserrat_28, 0);
    lv_obj_set_pos(lbl_temp[idx], 15, 45);

    // Temperature label
    lv_obj_t *t_label = lv_label_create(panel);
    lv_label_set_text(t_label, "Temp");
    lv_obj_set_style_text_color(t_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(t_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(t_label, 15, 80);

    // Humidity value
    lbl_hum[idx] = lv_label_create(panel);
    lv_label_set_text(lbl_hum[idx], "--.- %");
    lv_obj_set_style_text_color(lbl_hum[idx], lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_hum[idx], &lv_font_montserrat_28, 0);
    lv_obj_set_pos(lbl_hum[idx], 200, 45);

    // Humidity label
    lv_obj_t *h_label = lv_label_create(panel);
    lv_label_set_text(h_label, "Humidity");
    lv_obj_set_style_text_color(h_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(h_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(h_label, 200, 80);
}

// ============================================================
// Helper: create a chart
// ============================================================
static lv_obj_t *create_chart(lv_obj_t *parent, const char *title,
                               int y_pos, int y_min, int y_max,
                               lv_chart_series_t **s1, lv_chart_series_t **s2) {
    // Title
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, title);
    lv_obj_set_pos(lbl, 10, y_pos - 17);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_LABEL, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

    // Chart
    lv_obj_t *chart = lv_chart_create(parent);
    lv_obj_set_pos(chart, 5, y_pos);
    lv_obj_set_size(chart, 790, 140);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, CHART_POINTS);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
    lv_chart_set_div_line_count(chart, 5, 0);

    // Style
    lv_obj_set_style_bg_color(chart, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(chart, 0, 0);
    lv_obj_set_style_radius(chart, 8, 0);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);  // hide point dots

    // Series
    *s1 = lv_chart_add_series(chart, COLOR_S1_LINE, LV_CHART_AXIS_PRIMARY_Y);
    *s2 = lv_chart_add_series(chart, COLOR_S2_LINE, LV_CHART_AXIS_PRIMARY_Y);

    // Initialize with no-data markers
    for (int s = 0; s < CHART_POINTS; s++) {
        lv_chart_set_next_value(chart, *s1, LV_CHART_POINT_NONE);
        lv_chart_set_next_value(chart, *s2, LV_CHART_POINT_NONE);
    }

    return chart;
}

// ============================================================
// UI init
// ============================================================
void ui_init(void) {
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // Status bar
    lbl_status = lv_label_create(screen);
    lv_obj_set_pos(lbl_status, 10, 5);
    lv_label_set_text(lbl_status, "Connecting...");
    lv_obj_set_style_text_color(lbl_status, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);

    // Sensor panels
    create_sensor_panel(screen, 0, "Outdoor",   COLOR_S1_LINE, 4);
    create_sensor_panel(screen, 1, "Enclosure", COLOR_S2_LINE, 404);

    // Temperature chart (y range: -10 to 50 C, scaled x10 = -100 to 500)
    chart_temp = create_chart(screen, "Temperature (C)", 167, -100, 500,
                              &ser_temp[0], &ser_temp[1]);

    // Humidity chart (y range: 0 to 100%, scaled x10 = 0 to 1000)
    chart_hum = create_chart(screen, "Humidity (%)", 327, 0, 1000,
                             &ser_hum[0], &ser_hum[1]);

    lv_disp_load_scr(screen);
}

// ============================================================
// Update current readings display
// ============================================================
void ui_update_current(int sensorIdx, float temp, float humidity) {
    if (sensorIdx < 0 || sensorIdx > 1) return;

    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f C", temp);
    lv_label_set_text(lbl_temp[sensorIdx], buf);

    snprintf(buf, sizeof(buf), "%.1f %%", humidity);
    lv_label_set_text(lbl_hum[sensorIdx], buf);
}

// ============================================================
// Add data point to charts
// ============================================================
void ui_chart_add_point(int sensorIdx, float temp, float humidity) {
    if (sensorIdx < 0 || sensorIdx > 1) return;

    // Scale x10 for one-decimal precision
    lv_chart_set_next_value(chart_temp, ser_temp[sensorIdx], (lv_coord_t)(temp * 10));
    lv_chart_set_next_value(chart_hum,  ser_hum[sensorIdx],  (lv_coord_t)(humidity * 10));

    lv_chart_refresh(chart_temp);
    lv_chart_refresh(chart_hum);
}

// ============================================================
// Update status bar text
// ============================================================
void ui_set_status(const char* text) {
    lv_label_set_text(lbl_status, text);
}
