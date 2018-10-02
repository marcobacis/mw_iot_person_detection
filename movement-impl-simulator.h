/** @file 
 * @brief Accelerator Simulator Implementation
 * 
 * @author Marco Bacis
 * @author Daniele Cattaneo */

#include MOVEMENT_FILE

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

