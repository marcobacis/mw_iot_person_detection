/** @file 
 * @brief Configurable LED light pattern emitter
 *
 * This module contains utility functions for handling LED lights on the device
 * seamlessly in background, without any significant CPU cost.
 * 
 * @author Marco Bacis
 * @author Daniele Cattaneo */

#ifndef _LED_REPORT_H
#define _LED_REPORT_H

#include "contiki.h"
#include "dev/leds.h"

/* Duration of one LED time unit. */
#ifndef LED_CONF_PERIOD
#define LED_PERIOD (CLOCK_SECOND / 20)
#else
#define LED_PERIOD LED_CONF_PERIOD
#endif

/* Number of LEDs supported */
#ifndef LED_CONF_NUM_LEDS
#define NUM_LEDS   3
#else
#define NUM_LEDS   LED_CONF_NUM_LEDS
#endif


/** Initialize the led-report module.
 *
 * Must be called before any of the other functions in the module. */
void led_report_init(void);

/** Reset the specified LEDs to a given pattern.
 * @param leds    The LEDs affected.
 * @param pattern A bit vector specifying the pattern. If `t0` is the time this
 *                function is called, and if bit `n` is set, the LEDs will be on
 *                at time `t0+n*LED_PERIOD +/- LED_PERIOD`. Otherwise, if bit n
 *                is reset, at that time the LED will be off.
 * @param period  If not zero, the LED pattern set will be periodicized with
 *                a period of `period*LED_PERIOD`. Otherwise, the LED pattern
 *                will not be periodicized. 
 * @note When set_led_pattern is called for a set of LEDs which already had
 *       a pattern set previously, the new patterns will replace entirely
 *       the old patterns. */
void set_led_pattern(uint8_t leds, uint32_t pattern, uint8_t period);


#endif

