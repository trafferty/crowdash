// Hand-written following SquareLine Studio 1.6.0 conventions
// for crowdash project (LVGL 8.3.6)

#ifndef UI_SCREENSCHEDULE_H
#define UI_SCREENSCHEDULE_H

#ifdef __cplusplus
extern "C" {
#endif

// SCREEN: ui_screenschedule
extern void ui_screenschedule_screen_init(void);
extern void ui_screenschedule_screen_destroy(void);
extern lv_obj_t *ui_screenschedule;

// Left column — title and enable
extern lv_obj_t *ui_lblScheduleTitle;
extern lv_obj_t *ui_lblScheduleEnable;
extern lv_obj_t *ui_swtScheduleEnable;

// Sleep time panel
extern lv_obj_t *ui_pnlSleepTime;
extern lv_obj_t *ui_lblSleepLabel;
extern lv_obj_t *ui_txtSleepTime;
extern lv_obj_t *ui_lblSleepAmPm;
extern lv_obj_t *ui_swtSleepAmPm;

// Wake time panel
extern lv_obj_t *ui_pnlWakeTime;
extern lv_obj_t *ui_lblWakeLabel;
extern lv_obj_t *ui_txtWakeTime;
extern lv_obj_t *ui_lblWakeAmPm;
extern lv_obj_t *ui_swtWakeAmPm;

// Save button and navigation
extern lv_obj_t *ui_btnSaveSchedule;
extern lv_obj_t *ui_lblSave;
extern lv_obj_t *ui_btnGoToLoggerScreen;

// Right column — shared number keypad
extern lv_obj_t *ui_kbSchedule;

// Navigation event
extern void ui_event_btnGoToLoggerScreen(lv_event_t *e);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
