#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void ui_init(void);
void ui_update_current(int sensorIdx, float temp, float humidity);
void ui_chart_add_point(int sensorIdx, float temp, float humidity);
void ui_set_status(const char* text);

#ifdef __cplusplus
}
#endif

#endif
