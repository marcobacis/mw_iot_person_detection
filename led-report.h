#ifndef _LED_REPORT_H
#define _LED_REPORT_H

#include "contiki.h"
#include "dev/leds.h"


void led_report_init(void);

void set_led_pattern(uint8_t leds, uint32_t pattern, uint8_t period);


#endif

