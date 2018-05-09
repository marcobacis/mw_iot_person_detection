#include "contiki.h"
#include "sys/process.h"
#include <math.h>

/*
* Tells if there is a movement reading ready.
*/
int movement_ready(process_event_t ev, process_data_t data);

/*
* Inits the movement sensor (either real or fake)
*/
void init_movement_reading(void *not_used);

/*
* Gets the movement reading as a single value
* (reduction done by accumulating the absolute values)
*/
int get_movement();
