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

#define SENSOR_STARTUP_DELAY      5

FILE *mov_file;

process_event_t fakesens_event;

int movement_ready(process_event_t ev, process_data_t data) {
  return ev == fakesens_event;
}

void init_movement_reading(void *not_used) {
  
  static int opened = 0;
  
  if(!opened) {
    mov_file = fopen(MOVEMENT_FILE, "r");
    opened = 1;
  }
  
  process_post(PROCESS_BROADCAST, fakesens_event, NULL);
}

int get_movement() {
  
  int accx = 0;
  int accy = 0;
  int accz = 0;

  fscanf(mov_file, "%d,%d,%d\n", &accx, &accy, &accz);
  
  //reopen file if the end is reached
  if(feof(mov_file)) {
    fclose(mov_file);
    mov_file = fopen(MOVEMENT_FILE, "r");
    fscanf(mov_file, "%d,%d,%d\n", &accx, &accy, &accz);
  }
    

  return abs(accx)+abs(accy)+abs(accz);
}

#endif
