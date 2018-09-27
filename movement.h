#include "contiki.h"
#include "sys/process.h"
#include <math.h>
#include <stdio.h>


/* Last acceleration values read */
extern int last_acc[3];
#define LAST_ACC_X 0
#define LAST_ACC_Y 1
#define LAST_ACC_Z 2

/* Tells if there is a movement reading ready. */
int movement_ready(process_event_t ev, process_data_t data);

/* Inits the movement sensor (either real or fake) */
void init_movement_reading(void *not_used);

/* Gets the movement reading as a single value
 * (reduction done by accumulating the absolute values) */
int get_movement();
