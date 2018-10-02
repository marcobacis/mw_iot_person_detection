/** @file 
 * @brief Energest Log Module Implementation
 * 
 * @author Marco Bacis
 * @author Daniele Cattaneo */

#include "contiki.h"
#include "sys/energest.h"
#include "sys/log.h"
#include "project-conf.h"
#include "energest-log.h"


#define LOG_MODULE "Energy Log"
#ifdef LOG_CONF_LEVEL_PD_CLIENT
#define LOG_LEVEL LOG_CONF_LEVEL_ENERGEST_LOG
#else
#define LOG_LEVEL LOG_LEVEL_INFO
#endif


PROCESS(energest_process, "Energest print process");


PROCESS_THREAD(energest_process, ev, data)
{
  static struct etimer et;
  PROCESS_BEGIN();

  LOG_INFO("started\n");

  etimer_set(&et, ENERGEST_LOG_DELAY);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    etimer_reset(&et);

    /* Flush all energest times so we can read latest values */
    energest_flush();
    LOG_INFO("CPU: Active %lu LPM: %lu Deep LPM: %lu Total time: %lu seconds\n",
           (unsigned long)(energest_type_time(ENERGEST_TYPE_CPU) / ENERGEST_SECOND),
           (unsigned long)(energest_type_time(ENERGEST_TYPE_LPM) / ENERGEST_SECOND),
           (unsigned long)(energest_type_time(ENERGEST_TYPE_DEEP_LPM) / ENERGEST_SECOND),
           (unsigned long)(ENERGEST_GET_TOTAL_TIME() / ENERGEST_SECOND));
    LOG_INFO("Radio: Listen: %lu Transmit: %lu seconds\n",
           (unsigned long)(energest_type_time(ENERGEST_TYPE_LISTEN) / ENERGEST_SECOND),
           (unsigned long)(energest_type_time(ENERGEST_TYPE_TRANSMIT) / ENERGEST_SECOND));
  }
  PROCESS_END();
}

