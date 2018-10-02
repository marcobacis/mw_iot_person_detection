/** @file 
 * @brief Accelerator Reader Module Implementation
 *
 * This file implements both reading from the accelerometer of the cc2650
 * sensortag board and reading from an hardcoded array of values in the case
 * of the native target.
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


#if BOARD_SENSORTAG

#include "board-peripherals.h"

#define READING_ERROR CC26XX_SENSOR_READING_ERROR


int movement_ready(process_event_t ev, process_data_t data)
{
  return ev == sensors_event && data == &mpu_9250_sensor;
}


void init_movement_reading(void)
{
  mpu_9250_sensor.configure(SENSORS_ACTIVE, MPU_9250_SENSOR_TYPE_ACC);
}


int get_mvmt_value_reliable(int vid)
{
  int try = 5;
  int val = CC26XX_SENSOR_READING_ERROR;
  while (try > 0 && val == CC26XX_SENSOR_READING_ERROR) {
    val = mpu_9250_sensor.value(vid);
    try--;
  }
  if (val == CC26XX_SENSOR_READING_ERROR) {
    LOG_ERR("mvmt read failed\n");
  } else {
    LOG_DBG("mvmt read succeeded with %d retry attempts left\n", try);
  }
  return val;
}


void platform_get_movement(void)
{
  last_acc[LAST_ACC_X] = get_mvmt_value_reliable(MPU_9250_SENSOR_TYPE_ACC_X);
  last_acc[LAST_ACC_Y] = get_mvmt_value_reliable(MPU_9250_SENSOR_TYPE_ACC_Y);
  last_acc[LAST_ACC_Z] = get_mvmt_value_reliable(MPU_9250_SENSOR_TYPE_ACC_Z);
  
  LOG_INFO("mvmt read: %d %d %d\n", 
           last_acc[LAST_ACC_X], last_acc[LAST_ACC_Y], last_acc[LAST_ACC_Z]);
  SENSORS_DEACTIVATE(mpu_9250_sensor);
}


#else


#include MOVEMENT_FILE

#define SENSOR_STARTUP_DELAY      5
#define READING_ERROR             ((int)0x80000000)


process_event_t fakesens_event = PROCESS_EVENT_NONE;


int movement_ready(process_event_t ev, process_data_t data)
{
  return ev == fakesens_event;
}


void init_movement_reading(void)
{  
  if (fakesens_event == PROCESS_EVENT_NONE) {
    fakesens_event = process_alloc_event();
  }
  process_post(PROCESS_BROADCAST, fakesens_event, NULL);
}


void platform_get_movement(void)
{
  static int mov_idx = 0;

  last_acc[LAST_ACC_X] = movements[mov_idx][0];
  last_acc[LAST_ACC_Y] = movements[mov_idx][1];
  last_acc[LAST_ACC_Z] = movements[mov_idx][2];

  if(mov_idx < MOVEMENTS-1) {
    mov_idx++;
  } else {
    mov_idx = 0;
  }
}


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


