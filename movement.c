#include "movement.h"

#if BOARD_SENSORTAG

#include "board-peripherals.h"

int movement_ready(process_event_t ev, process_data_t data) {
  return ev == sensors_event && data == &mpu_9250_sensor
}

void init_movement_reading(void *not_used) {
  mpu_9250_sensor.configure(SENSORS_ACTIVE, MPU_9250_SENSOR_TYPE_ACC);
}

int get_movement() {
  int accx, accy, accz;
  clock_time_t next = SENSOR_READING_PERIOD;


  accx = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_ACC_X);
  accy = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_ACC_Y);
  accz = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_ACC_Z);

  SENSORS_DEACTIVATE(mpu_9250_sensor);

  return abs(accx)+abs(accy)+abs(accz);
}

#else

#include MOVEMENT_FILE

#define SENSOR_STARTUP_DELAY      5

process_event_t fakesens_event;

int movement_ready(process_event_t ev, process_data_t data) {
  return ev == fakesens_event;
}

void init_movement_reading(void *not_used) {
  
  process_post(PROCESS_BROADCAST, fakesens_event, NULL);
}

int get_movement() {
  
  static int mov_idx = 0;

  int accx = movements[mov_idx][0];
  int accy = movements[mov_idx][1];
  int accz = movements[mov_idx][2];

  if(mov_idx < MOVEMENTS-1) {
    mov_idx++;
  } else {
    mov_idx = 0;
  }
    

  return abs(accx)+abs(accy)+abs(accz);
}

#endif
