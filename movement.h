/** @file 
 * @brief Accelerometer Reader Module
 *
 * This module contains all the functions which interact 
 * directly with the accelerometer (which is real only for targets that 
 * have it, otherwise it's a simulation).
 * 
 * @author Marco Bacis
 * @author Daniele Cattaneo */

#include "contiki.h"
#include "sys/process.h"
#include <math.h>
#include <stdio.h>


/** Last acceleration values read. */
extern int last_acc[3];

/** The index in last_acc[] corresponding to the X axis. */
#define LAST_ACC_X 0
/** The index in last_acc[] corresponding to the Y axis. */
#define LAST_ACC_Y 1
/** The index in last_acc[] corresponding to the Z axis. */
#define LAST_ACC_Z 2

/** The modulo of the gravity acceleration. 
 *
 * Sets the scale of the values in last_acc and returned by get_movement(). */
#define GRAVITY 100


/** Inits the movement sensor.
 * 
 * This function must be called before any attempt to use get_movement().
 * After an unspecified period of time, a broadcast event will be delivered,
 * which notifies that the accelerator can be read. Use movement_ready()
 * to identify that message. */
void init_movement_reading(void);

/** Tells if there is a movement reading ready. 
 * @param ev    An event identifier.
 * @param data  The data of event `ev`.
 * @returns 1 if `ev` and `data` indicate that the accelerator can now be
 *          accessed (in other words, get_movement() can be called). */
int movement_ready(process_event_t ev, process_data_t data);

/** Gets the movement reading as a single value.
 * @returns The modulo squared of the acceleration vector read from the 
 *          accelerometer, or -1 if an error occurred.
 * @note    This function also updates last_acc with the acceleration values
 *          read from the sensor. If get_movement() returns -1, the values in
 *          last_acc are undefined.
 * @warning This function turns off the accelerator before returning. Thus,
 *          to read it again, you must call init_movement_reading() and
 *          wait for the readiness events with movement_ready() again. */
int get_movement();


