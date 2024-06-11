#ifndef LV_LUMAMB_H
#define LV_LUMAMB_H
#include "lvgl.h"

void switch_event_intensite(lv_event_t * e);
void switch_event_gps(lv_event_t * e);
void button_event_cb(lv_event_t * e);
void slider_event_cb(lv_event_t * e); 
void setup_ui() ;
void parseNMEA(const char* nmea);

#endif