#include "contiki.h"
#include "sys/log.h"
#include "led-report.h"


#define LOG_MODULE "Leds"
#define LOG_LEVEL LOG_LEVEL_INFO

#define LED_PERIOD (CLOCK_SECOND / 8)
#define NUM_LEDS   3


PROCESS(led_report_process, "LED Handler");


typedef struct {
  uint32_t pattern;
  uint8_t period;
} led_pattern_info_t;

#define ROTATE_PATTERN(pi, n) do { \
    (pi).pattern = ((pi).pattern | ((pi).pattern << (pi).period)) >> (n); \
  } while (0)


led_pattern_info_t led_patterns[NUM_LEDS];
struct etimer led_timer;


void set_led_pattern(uint8_t leds, uint32_t pattern, uint8_t period)
{
  PROCESS_CONTEXT_BEGIN(&led_report_process);
  leds &= (1 << NUM_LEDS) - 1;
  
  int i = 0;
  while (leds != 0) {
    if (leds & 1) {
      led_patterns[i].pattern = pattern;
      led_patterns[i].period = period; 
    }
    i++;
    leds >>= 1;
  }
  
  etimer_set(&led_timer, 0);
  PROCESS_CONTEXT_END(&led_report_process);
}


PROCESS_THREAD(led_report_process, ev, data)
{
  PROCESS_BEGIN();
  
  leds_init();
  
  while (1) {
    unsigned char leds_state = 0;
    uint32_t all_transitions = 0;
    
    for (int i=0; i<NUM_LEDS; i++) {
      int32_t this_led_state = led_patterns[i].pattern & 1;
      leds_state |= (unsigned char)this_led_state << i;
      ROTATE_PATTERN(led_patterns[i], 1);
      all_transitions |= led_patterns[i].pattern ^ (uint32_t)(-this_led_state);
    }
    
    leds_set(leds_state);
    LOG_DBG("led state changed to %d%d%d\n", leds_state & 1, (leds_state >> 1) & 1, (leds_state >> 2) & 1);
    
    if (all_transitions != 0) {
      int wait_n = 0;
      while ((all_transitions & 1) == 0) {
        all_transitions >>= 1;
        wait_n++;
      } 
  
      etimer_set(&led_timer, LED_PERIOD * (wait_n + 1));
      for (int i=0; i<NUM_LEDS; i++) {
        ROTATE_PATTERN(led_patterns[i], wait_n);
      }
    }
    
    do {
      PROCESS_YIELD();
    } while (ev != PROCESS_EVENT_TIMER);
  }
  
  PROCESS_END();
}

