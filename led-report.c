#include "contiki.h"
#include "sys/log.h"
#include "led-report.h"


#define LOG_MODULE "Leds"
#ifdef LOG_CONF_LEVEL_LED_REPORT
#define LOG_LEVEL  LOG_CONF_LEVEL_LED_REPORT
#else
#ifdef CONTIKI_TARGET_NATIVE
#define LOG_LEVEL  LOG_LEVEL_DBG
#else
#define LOG_LEVEL  LOG_LEVEL_INFO
#endif
#endif

#ifdef CONTIKI_TARGET_NATIVE
//#define FANCY_PRINTF
#endif

#ifndef ENABLE_LEDS
#define ENABLE_LEDS 1
#endif


// duration of a quantum (fastest possible LED blink)
#define LED_PERIOD (CLOCK_SECOND / 20)
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
clock_time_t t_last_shift = 0;
clock_time_t t_last_update = 0;


void set_led_pattern(uint8_t leds, uint32_t pattern, uint8_t period)
{
  #if ENABLE_LEDS == 0
  return;
  #endif
  
  PROCESS_CONTEXT_BEGIN(&led_report_process);
  
  leds &= (1 << NUM_LEDS) - 1;
  
  clock_time_t t_elapsed = MAX(0, clock_time() - t_last_shift);
  clock_time_t t_remainder = t_elapsed % LED_PERIOD;
  int next_quantum = (t_elapsed + LED_PERIOD - 1) / LED_PERIOD;
  
  for (int i=0; i<NUM_LEDS; i++) {
    ROTATE_PATTERN(led_patterns[i], next_quantum);
    
    if (leds & 1) {
      led_patterns[i].pattern = pattern;
      led_patterns[i].period = period;
    }
    leds >>= 1;
  }
  
  t_last_shift += next_quantum * LED_PERIOD;
  if (t_elapsed > 0) {
    etimer_set(&led_timer, LED_PERIOD - t_remainder);
  }
  
  PROCESS_CONTEXT_END(&led_report_process);
}


void led_report_init(void)
{
  #if ENABLE_LEDS == 0
  return;
  #endif
  
  leds_init();
  memset(led_patterns, 0, sizeof(led_pattern_info_t) * NUM_LEDS);
  process_start(&led_report_process, NULL);
  
  PROCESS_CONTEXT_BEGIN(&led_report_process);
  etimer_set(&led_timer, 0);
  t_last_shift = etimer_start_time(&led_timer);
  t_last_update = etimer_start_time(&led_timer);
  PROCESS_CONTEXT_END(&led_report_process);
}


PROCESS_THREAD(led_report_process, ev, data)
{
  PROCESS_BEGIN();
  
  while (ev != PROCESS_EVENT_EXIT) {
    PROCESS_YIELD();
    if (!etimer_expired(&led_timer))
      continue;
      
    clock_time_t t_this_update = etimer_expiration_time(&led_timer);
    int shift_amt = (t_this_update - t_last_shift) / LED_PERIOD;
  
    unsigned char leds_state = 0;
    uint32_t all_transitions = 0;
    
    for (int i=0; i<NUM_LEDS; i++) {
      ROTATE_PATTERN(led_patterns[i], shift_amt);
      
      int32_t this_led_state = led_patterns[i].pattern & 1;
      leds_state |= (unsigned char)this_led_state << i;
      all_transitions |= led_patterns[i].pattern ^ (uint32_t)(-this_led_state);
    }
    
    leds_set(leds_state);
    t_last_shift = t_last_update = t_this_update;
    
    int dt_next_update = 0;
    if (all_transitions != 0) {
      while ((all_transitions & 1) == 0) {
        all_transitions >>= 1;
        dt_next_update += LED_PERIOD;
      } 
  
      etimer_reset_with_new_interval(&led_timer, dt_next_update);
    }
    
    #ifdef FANCY_PRINTF
    printf("\0337\033[1;1H\033[33m[ LEDS : %d%d%d ]\033[39m\0338", 
           leds_state & 1, (leds_state >> 1) & 1, (leds_state >> 2) & 1);
    #else
    LOG_DBG("led state = %d%d%d, t = %d, next = %d\n", 
            leds_state & 1, (leds_state >> 1) & 1, (leds_state >> 2) & 1,
            (int)clock_time(), dt_next_update);
    #endif
  }
  
  leds_set(0);
  
  PROCESS_END();
}

