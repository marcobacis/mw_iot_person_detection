#include "contiki.h"
#include "sys/log.h"
#include "led-report.h"


#define LOG_MODULE "Leds"
#ifdef CONTIKI_TARGET_NATIVE
#define LOG_LEVEL LOG_LEVEL_DBG
#else
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#define FANCY_PRINTF

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
int wait_quantums = 0;


void set_led_pattern(uint8_t leds, uint32_t pattern, uint8_t period)
{
  PROCESS_CONTEXT_BEGIN(&led_report_process);
  leds &= (1 << NUM_LEDS) - 1;
  
  clock_time_t elapsed_t = MAX(0, etimer_expiration_time(&led_timer) - clock_time());
  int elapsed_quantums = elapsed_t / LED_PERIOD;
  int remainder = elapsed_t % LED_PERIOD;
  
  for (int i=0; i<NUM_LEDS; i++) {
    ROTATE_PATTERN(led_patterns[i], elapsed_quantums);
    
    if (leds & 1) {
      led_patterns[i].pattern = pattern;
      led_patterns[i].period = period;
    }
    leds >>= 1;
  }
  
  wait_quantums = 0;
  etimer_set(&led_timer, remainder);
  PROCESS_CONTEXT_END(&led_report_process);
}


void led_report_init(void)
{
  leds_init();
  memset(led_patterns, 0, sizeof(led_pattern_info_t) * NUM_LEDS);
  process_start(&led_report_process, NULL);
}


PROCESS_THREAD(led_report_process, ev, data)
{
  PROCESS_BEGIN();
  
  while (1) {
    unsigned char leds_state = 0;
    uint32_t all_transitions = 0;
    
    for (int i=0; i<NUM_LEDS; i++) {
      ROTATE_PATTERN(led_patterns[i], wait_quantums);
      
      int32_t this_led_state = led_patterns[i].pattern & 1;
      leds_state |= (unsigned char)this_led_state << i;
      all_transitions |= led_patterns[i].pattern ^ (uint32_t)(-this_led_state);
    }
    
    leds_set(leds_state);
    
    #ifdef FANCY_PRINTF
    printf("\0337\033[1;1H\033[33m[ LEDS : %d%d%d ]\033[39m\0338", leds_state & 1, (leds_state >> 1) & 1, (leds_state >> 2) & 1);
    #else
    LOG_DBG("led state = %d%d%d\n", leds_state & 1, (leds_state >> 1) & 1, (leds_state >> 2) & 1);
    #endif
    
    wait_quantums = 0;
    if (all_transitions != 0) {
      while ((all_transitions & 1) == 0) {
        all_transitions >>= 1;
        wait_quantums++;
      } 
  
      etimer_set(&led_timer, LED_PERIOD * (wait_quantums + 1));
    }
    
    do {
      PROCESS_YIELD();
    } while (ev != PROCESS_EVENT_TIMER);
  }
  
  PROCESS_END();
}

