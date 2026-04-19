// Hand-written following SquareLine Studio 1.6.0 conventions
// for crowdash project (LVGL 8.3.6)
// Screen order: Dashboard | Timer | Logger | Schedule

#include "ui.h"

// Widget globals — all NULL'd in destroy
lv_obj_t *ui_screenschedule      = NULL;
lv_obj_t *ui_lblScheduleTitle    = NULL;
lv_obj_t *ui_lblScheduleEnable   = NULL;
lv_obj_t *ui_swtScheduleEnable   = NULL;
lv_obj_t *ui_pnlSleepTime        = NULL;
lv_obj_t *ui_lblSleepLabel       = NULL;
lv_obj_t *ui_txtSleepTime        = NULL;
lv_obj_t *ui_lblSleepAmPm        = NULL;
lv_obj_t *ui_swtSleepAmPm        = NULL;
lv_obj_t *ui_pnlWakeTime         = NULL;
lv_obj_t *ui_lblWakeLabel        = NULL;
lv_obj_t *ui_txtWakeTime         = NULL;
lv_obj_t *ui_lblWakeAmPm         = NULL;
lv_obj_t *ui_swtWakeAmPm         = NULL;
lv_obj_t *ui_btnSaveSchedule     = NULL;
lv_obj_t *ui_lblSave             = NULL;
lv_obj_t *ui_btnGoToLoggerScreen = NULL;
lv_obj_t *ui_kbSchedule          = NULL;

// Navigate back to Logger (Schedule is rightmost screen)
void ui_event_btnGoToLoggerScreen(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        _ui_screen_change(&ui_screenlogger, LV_SCR_LOAD_ANIM_MOVE_LEFT,
                          100, 0, &ui_screenlogger_screen_init);
    }
}

// ── Helper: build a time-entry panel ─────────────────────────────────────────
// Creates a panel with: label | textarea | AM/PM label + switch
// Returns the panel; out_ta and out_swt receive the textarea and switch.
static lv_obj_t *make_time_panel(lv_obj_t *parent,
                                  int32_t x, int32_t y,
                                  const char *row_label,
                                  const char *default_hhmm,
                                  bool default_pm,
                                  lv_obj_t **out_ta,
                                  lv_obj_t **out_ampm_lbl,
                                  lv_obj_t **out_swt)
{
    lv_obj_t *pnl = lv_obj_create(parent);
    lv_obj_set_width(pnl, 340);
    lv_obj_set_height(pnl, 80);
    lv_obj_set_x(pnl, x);
    lv_obj_set_y(pnl, y);
    lv_obj_set_align(pnl, LV_ALIGN_CENTER);
    lv_obj_clear_flag(pnl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(pnl, lv_color_hex(0x16213E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(pnl, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(pnl, lv_color_hex(0x444466), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(pnl, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(pnl, 6, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Row label (left)
    lv_obj_t *lbl = lv_label_create(pnl);
    lv_obj_set_x(lbl, -110);
    lv_obj_set_y(lbl, 0);
    lv_obj_set_align(lbl, LV_ALIGN_CENTER);
    lv_label_set_text(lbl, row_label);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);

    // HHMM textarea (centre)
    lv_obj_t *ta = lv_textarea_create(pnl);
    lv_obj_set_width(ta, 110);
    lv_obj_set_height(ta, LV_SIZE_CONTENT);
    lv_obj_set_x(ta, 10);
    lv_obj_set_y(ta, 0);
    lv_obj_set_align(ta, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ta, 4);
    lv_textarea_set_text(ta, default_hhmm);
    lv_textarea_set_one_line(ta, true);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ta, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ta, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x565252), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ta, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    *out_ta = ta;

    // AM/PM label above switch (right)
    lv_obj_t *ampm_lbl = lv_label_create(pnl);
    lv_obj_set_x(ampm_lbl, 130);
    lv_obj_set_y(ampm_lbl, -16);
    lv_obj_set_align(ampm_lbl, LV_ALIGN_CENTER);
    lv_label_set_text(ampm_lbl, "AM/PM");
    lv_obj_set_style_text_font(ampm_lbl, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ampm_lbl, lv_color_hex(0xAAAAAA), LV_PART_MAIN | LV_STATE_DEFAULT);
    *out_ampm_lbl = ampm_lbl;

    // AM/PM switch (right, below label)
    lv_obj_t *swt = lv_switch_create(pnl);
    lv_obj_set_width(swt, 56);
    lv_obj_set_height(swt, 30);
    lv_obj_set_x(swt, 130);
    lv_obj_set_y(swt, 12);
    lv_obj_set_align(swt, LV_ALIGN_CENTER);
    if (default_pm) lv_obj_add_state(swt, LV_STATE_CHECKED);
    *out_swt = swt;

    return pnl;
}

void ui_screenschedule_screen_init(void)
{
    // ── Root screen ──────────────────────────────────────────────────────────
    ui_screenschedule = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_screenschedule, LV_OBJ_FLAG_SCROLLABLE);

    // ── Title ─────────────────────────────────────────────────────────────────
    ui_lblScheduleTitle = lv_label_create(ui_screenschedule);
    lv_obj_set_x(ui_lblScheduleTitle, -160);
    lv_obj_set_y(ui_lblScheduleTitle, -205);
    lv_obj_set_align(ui_lblScheduleTitle, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblScheduleTitle, "Display Schedule");
    lv_obj_set_style_text_font(ui_lblScheduleTitle, &lv_font_montserrat_28,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblScheduleTitle, lv_color_hex(0xFFFFFF),
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    // ── Enable row ───────────────────────────────────────────────────────────
    ui_lblScheduleEnable = lv_label_create(ui_screenschedule);
    lv_obj_set_x(ui_lblScheduleEnable, -310);
    lv_obj_set_y(ui_lblScheduleEnable, -148);
    lv_obj_set_align(ui_lblScheduleEnable, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblScheduleEnable, "Enable Schedule");
    lv_obj_set_style_text_font(ui_lblScheduleEnable, &lv_font_montserrat_20,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblScheduleEnable, lv_color_hex(0xCCCCCC),
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_swtScheduleEnable = lv_switch_create(ui_screenschedule);
    lv_obj_set_width(ui_swtScheduleEnable, 68);
    lv_obj_set_height(ui_swtScheduleEnable, 38);
    lv_obj_set_x(ui_swtScheduleEnable, -120);
    lv_obj_set_y(ui_swtScheduleEnable, -148);
    lv_obj_set_align(ui_swtScheduleEnable, LV_ALIGN_CENTER);
    // Default: unchecked (schedule off)

    // ── Sleep time panel (default 10:00 PM → 2200) ───────────────────────────
    ui_pnlSleepTime = make_time_panel(ui_screenschedule,
                                      -160, -65,
                                      "Sleep at",
                                      "1000", true,
                                      &ui_txtSleepTime,
                                      &ui_lblSleepAmPm,
                                      &ui_swtSleepAmPm);
    ui_lblSleepLabel = lv_obj_get_child(ui_pnlSleepTime, 0);

    // ── Wake time panel (default 7:00 AM → 0700) ─────────────────────────────
    ui_pnlWakeTime = make_time_panel(ui_screenschedule,
                                     -160, 35,
                                     "Wake at",
                                     "0700", false,
                                     &ui_txtWakeTime,
                                     &ui_lblWakeAmPm,
                                     &ui_swtWakeAmPm);
    ui_lblWakeLabel = lv_obj_get_child(ui_pnlWakeTime, 0);

    // ── Save button ──────────────────────────────────────────────────────────
    ui_btnSaveSchedule = lv_btn_create(ui_screenschedule);
    lv_obj_set_width(ui_btnSaveSchedule, 200);
    lv_obj_set_height(ui_btnSaveSchedule, 60);
    lv_obj_set_x(ui_btnSaveSchedule, -220);
    lv_obj_set_y(ui_btnSaveSchedule, 130);
    lv_obj_set_align(ui_btnSaveSchedule, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_btnSaveSchedule, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_btnSaveSchedule, lv_color_hex(0x0C0D60),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_btnSaveSchedule, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblSave = lv_label_create(ui_btnSaveSchedule);
    lv_obj_set_align(ui_lblSave, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblSave, "Save");
    lv_obj_set_style_text_font(ui_lblSave, &lv_font_montserrat_28,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblSave, lv_color_hex(0xFFFFFF),
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    // ── Left-arrow nav button (back to Logger) ───────────────────────────────
    ui_btnGoToLoggerScreen = lv_imgbtn_create(ui_screenschedule);
    lv_imgbtn_set_src(ui_btnGoToLoggerScreen, LV_IMGBTN_STATE_RELEASED,
                      NULL, &ui_img_lt_arrow_png, NULL);
    lv_imgbtn_set_src(ui_btnGoToLoggerScreen, LV_IMGBTN_STATE_PRESSED,
                      NULL, &ui_img_lt_arrow_png, NULL);
    lv_obj_set_width(ui_btnGoToLoggerScreen, 64);
    lv_obj_set_height(ui_btnGoToLoggerScreen, 64);
    lv_obj_set_x(ui_btnGoToLoggerScreen, -356);
    lv_obj_set_y(ui_btnGoToLoggerScreen, 194);
    lv_obj_set_align(ui_btnGoToLoggerScreen, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_btnGoToLoggerScreen, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_btnGoToLoggerScreen, lv_color_hex(0x4D4F5B),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_btnGoToLoggerScreen, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_color(ui_btnGoToLoggerScreen, lv_color_hex(0x000000),
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(ui_btnGoToLoggerScreen, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ── Shared number keypad (right column, same geometry as timer screen) ───
    ui_kbSchedule = lv_keyboard_create(ui_screenschedule);
    lv_keyboard_set_mode(ui_kbSchedule, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_width(ui_kbSchedule, 441);
    lv_obj_set_height(ui_kbSchedule, 404);
    lv_obj_set_x(ui_kbSchedule, 161);
    lv_obj_set_y(ui_kbSchedule, 40);
    lv_obj_set_align(ui_kbSchedule, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_kbSchedule, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_style_text_font(ui_kbSchedule, &lv_font_montserrat_48,
                               LV_PART_ITEMS | LV_STATE_DEFAULT);

    // Default focus: sleep textarea
    lv_keyboard_set_textarea(ui_kbSchedule, ui_txtSleepTime);

    // Wire navigation button
    lv_obj_add_event_cb(ui_btnGoToLoggerScreen,
                        ui_event_btnGoToLoggerScreen,
                        LV_EVENT_ALL, NULL);
}

void ui_screenschedule_screen_destroy(void)
{
    if (ui_screenschedule) lv_obj_del(ui_screenschedule);
    ui_screenschedule      = NULL;
    ui_lblScheduleTitle    = NULL;
    ui_lblScheduleEnable   = NULL;
    ui_swtScheduleEnable   = NULL;
    ui_pnlSleepTime        = NULL;
    ui_lblSleepLabel       = NULL;
    ui_txtSleepTime        = NULL;
    ui_lblSleepAmPm        = NULL;
    ui_swtSleepAmPm        = NULL;
    ui_pnlWakeTime         = NULL;
    ui_lblWakeLabel        = NULL;
    ui_txtWakeTime         = NULL;
    ui_lblWakeAmPm         = NULL;
    ui_swtWakeAmPm         = NULL;
    ui_btnSaveSchedule     = NULL;
    ui_lblSave             = NULL;
    ui_btnGoToLoggerScreen = NULL;
    ui_kbSchedule          = NULL;
}
