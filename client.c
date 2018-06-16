/**	
 * \file
 *         Client-side application for the person detection project
 * 
 * Authors: Marco Bacis, Daniele Cattaneo
 */

#include "contiki.h"
#include "sys/process.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "movement.h"
#include <stdio.h>

static struct ctimer acc_timer;


/*---------------------------------------------------------------------------*/
PROCESS(client_process, "Hello world process");

#include "mqtt_lib.h"

AUTOSTART_PROCESSES(&client_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(client_process, ev, data)
{

  PROCESS_BEGIN();

  printf("Hello, world\n");
  
  init_movement_reading(NULL);

  init_mqtt_config();
  update_mqtt_config();
  
  while(1) {
    PROCESS_YIELD();
    
    if(movement_ready(ev, data)) {
      printf("Read movement of %d Gs\n", get_movement());
      ctimer_set(&acc_timer, MOVEMENT_PERIOD, init_movement_reading, NULL);
      state_machine();
    }
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
