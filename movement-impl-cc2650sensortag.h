/** @file 
 * @brief Accelerator Reader Implementation for CC2650 Sensortag
 * 
 * @author Marco Bacis
 * @author Daniele Cattaneo */
 
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
