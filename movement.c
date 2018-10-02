/** @file 
 * @brief Accelerometer Reader Module Implementation
 *
 * This file implements the common interface of the accelerator reader.
 * 
 * @author Marco Bacis
 * @author Daniele Cattaneo */
 
#include "movement.h"
#include "sys/log.h"


#define LOG_MODULE "Movement"
#ifdef LOG_CONF_LEVEL_MOVEMENT
#define LOG_LEVEL LOG_CONF_LEVEL_MOVEMENT
#else
#define LOG_LEVEL LOG_LEVEL_INFO
#endif


int last_acc[3];

/** Updates last_acc[] with the accelerometer readings; then turn off the
 * accelerometer. */
void platform_get_movement(void);


/* Each platform-dependent implementation must:
 *  - implement init_movement_reading()
 *  - implement movement_ready()
 *  - implement platform_get_movement() 
 *  - define the value of READING_ERROR */
 
#if BOARD_SENSORTAG
#include "movement-impl-cc2650sensortag.h"
#else
#include "movement-impl-simulator.h"
#endif


int get_movement()
{
  platform_get_movement();
  
  if (last_acc[LAST_ACC_X] == READING_ERROR || 
      last_acc[LAST_ACC_Y] == READING_ERROR || 
      last_acc[LAST_ACC_Z] == READING_ERROR) {
    return -1;
  }
  
  return last_acc[LAST_ACC_X]*last_acc[LAST_ACC_X] + 
         last_acc[LAST_ACC_Y]*last_acc[LAST_ACC_Y] + 
         last_acc[LAST_ACC_Z]*last_acc[LAST_ACC_Z];
}


