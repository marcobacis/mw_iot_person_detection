#include "movement.h"
#include "sys/log.h"


#define LOG_MODULE "MVMT"
#define LOG_LEVEL LOG_LEVEL_DBG


#if BOARD_SENSORTAG

#include "board-peripherals.h"


int movement_ready(process_event_t ev, process_data_t data)
{
  return ev == sensors_event && data == &mpu_9250_sensor;
}


void init_movement_reading(void *not_used)
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
    LOG_INFO("mvmt read failed\n");
  } else {
    LOG_DBG("mvmt read succeeded with %d retry attempts left\n", try);
  }
  return val;
}


int get_movement() {
  int accx, accy, accz;

  accx = get_mvmt_value_reliable(MPU_9250_SENSOR_TYPE_ACC_X);
  accy = get_mvmt_value_reliable(MPU_9250_SENSOR_TYPE_ACC_Y);
  accz = get_mvmt_value_reliable(MPU_9250_SENSOR_TYPE_ACC_Z);
  if (accx == CC26XX_SENSOR_READING_ERROR || accy == CC26XX_SENSOR_READING_ERROR || accz == CC26XX_SENSOR_READING_ERROR)
    return -1;
  
  LOG_INFO("mvmt read: %d %d %d\n", accx, accy, accz);

  SENSORS_DEACTIVATE(mpu_9250_sensor);

  return accx*accx + accy*accy + accz*accz;
}


#else


#include MOVEMENT_FILE


#define SENSOR_STARTUP_DELAY      5


process_event_t fakesens_event = PROCESS_EVENT_NONE;


int movement_ready(process_event_t ev, process_data_t data)
{
  return ev == fakesens_event;
}


void init_movement_reading(void *not_used)
{  
  if (fakesens_event == PROCESS_EVENT_NONE) {
    fakesens_event = process_alloc_event();
  }
  process_post(PROCESS_BROADCAST, fakesens_event, NULL);
}


int get_movement()
{
  static int mov_idx = 0;

  int accx = movements[mov_idx][0];
  int accy = movements[mov_idx][1];
  int accz = movements[mov_idx][2];

  if(mov_idx < MOVEMENTS-1) {
    mov_idx++;
  } else {
    mov_idx = 0;
  }
    
  return accx*accx + accy*accy + accz*accz;
}


#endif
