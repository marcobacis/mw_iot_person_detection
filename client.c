/**	
 * \file
 *         Client-side application for the person detection project
 * 
 * Authors: Marco Bacis, Daniele Cattaneo
 */

#include "contiki.h"
#include "rpl.h"
#include "sys/process.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "sys/energest.h"
#include "movement.h"
#include "sys/clock.h"
#include "dev/leds.h"
#include <stdio.h>


static uint8_t state;
#define STATE_INIT            0
#define STATE_REGISTERED      1
#define STATE_CONNECTING      2
#define STATE_CONNECTED       3
#define STATE_PUBLISHING      4
#define STATE_DISCONNECTED    5
#define STATE_NEWCONFIG       6
#define STATE_MOVING          7
#define STATE_INIT_MOVE       8
#define STATE_CONFIG_ERROR 0xFE
#define STATE_ERROR        0xFF


static struct ctimer acc_timer;


PROCESS(client_process, "mw-iot-person-detection process");

#if ENERGEST_CONF_ON == 1
  PROCESS(energest_process, "Energest print process");
  AUTOSTART_PROCESSES(&client_process,&energest_process);
#else
  AUTOSTART_PROCESSES(&client_process);
#endif


#include "mqtt_lib.h"


void blink_led(unsigned char ledv)
{
  leds_on(ledv);
  clock_delay(400);
  leds_off(ledv);
}


static void
state_machine(void)
{
  switch(state) {

    case STATE_MOVING:
      /* Dummy state, all the checks are done in the main process */
      etimer_set(&publish_periodic_timer, STATE_MACHINE_PERIODIC);
      return;

    case STATE_INIT:
      /* If we have just been configured register MQTT connection */

      mqtt_register(&conn, &client_process, client_id, mqtt_event,
                    MAX_TCP_SEGMENT_SIZE);

      mqtt_set_username_password(&conn, "use-token-auth",
                                     conf.auth_token);

      /* _register() will set auto_reconnect. We don't want that. */
      conn.auto_reconnect = 0;
      connect_attempt = 1;

      state = STATE_REGISTERED;
      LOG_INFO("Init\n");

    case STATE_REGISTERED:
      if(uip_ds6_get_global(ADDR_PREFERRED) != NULL) {
        /* Registered and with a global IPv6 address, connect! */
        update_mqtt_config();
        LOG_INFO("Joined network! Connect attempt %u\n", connect_attempt);
        connect_to_broker();
      }
      etimer_set(&publish_periodic_timer, NET_CONNECT_PERIODIC);
      return;
      break;

    case STATE_CONNECTING:
      blink_led(LEDS_RED);
      LOG_INFO("Connecting: retry %u...\n", connect_attempt);
      break;

    case STATE_CONNECTED:
      update_mqtt_config();
      state = STATE_PUBLISHING;


    case STATE_PUBLISHING:
      blink_led(LEDS_RED | LEDS_GREEN);
      /* If the timer expired, the connection is stable. */
      if(timer_expired(&connection_life)) {
        /*
         * Intentionally using 0 here instead of 1: We want RECONNECT_ATTEMPTS
         * attempts if we disconnect after a successful connect
         */
        connect_attempt = 0;
      }

      LOG_INFO("Should publish\n");
      if(mqtt_ready(&conn) && conn.out_buffer_sent) {
        /* Connected. Publish */
        publish();
        etimer_set(&publish_periodic_timer, conf.pub_interval);

        /* Return here so we don't end up rescheduling the timer */
        return;
      } else {
        /*
         * Our publish timer fired, but some MQTT packet is already in flight
         * (either not sent at all, or sent but not fully ACKd).
         *
         * This can mean that we have lost connectivity to our broker or that
         * simply there is some network delay. In both cases, we refuse to
         * trigger a new message and we wait for TCP to either ACK the entire
         * packet after retries, or to timeout and notify us.
         */
        
        if(!mqtt_connected(&conn) || !rpl_is_reachable()) {
          LOG_INFO("mqtt disconnected...\n");
          state = STATE_DISCONNECTED;
        } else {
          LOG_INFO("Publishing... (MQTT state=%d, q=%u)\n", conn.state,
            conn.out_queue_full);
        }

      }
      break;

    case STATE_DISCONNECTED:
      blink_led(LEDS_RED);
      LOG_INFO("Disconnected\n");
      if(connect_attempt < RECONNECT_ATTEMPTS ||
         RECONNECT_ATTEMPTS == RETRY_FOREVER) {
        /* Disconnect and backoff */
        clock_time_t interval;
        mqtt_disconnect(&conn);
        connect_attempt++;

        interval = connect_attempt < 3 ? RECONNECT_INTERVAL << connect_attempt :
          RECONNECT_INTERVAL << 3;

        LOG_INFO("Disconnected: attempt %u in %lu ticks\n", connect_attempt, interval);

        etimer_set(&publish_periodic_timer, interval);

        state = STATE_REGISTERED;
        return;
      } else {
        /* Max reconnect attempts reached. Restart movement checking*/
        state = STATE_INIT_MOVE;
        LOG_ERR("Keep movin'\n");
      }
      break;

    case STATE_INIT_MOVE:
      mqtt_disconnect(&conn);
      state = STATE_MOVING;
      LOG_INFO("Restarting movement.\n");
      break;

    case STATE_CONFIG_ERROR:
      /* Idle away. The only way out is a new config */
      LOG_ERR("Bad configuration.\n");
      return;

    case STATE_ERROR:
    default:
      /*
       * 'default' should never happen.
       *
       * If we enter here it's because of some error. Stop timers. The only thing
       * that can bring us out is a new config event
       */
      LOG_INFO("Default case: State=0x%02x\n", state);
      return;
  }

  /* If we didn't return so far, reschedule ourselves */
  etimer_set(&publish_periodic_timer, STATE_MACHINE_PERIODIC);
}


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(client_process, ev, data)
{
  leds_init();
  
  PROCESS_BEGIN();
  log_set_level("main", LOG_LEVEL_DBG);
  log_set_level("tcpip", LOG_LEVEL_DBG);
  
  init_movement_reading(NULL);

  state = STATE_MOVING;

  init_mqtt();
  init_mqtt_config();
  update_mqtt_config();

  etimer_set(&publish_periodic_timer, STATE_MACHINE_PERIODIC);
  
  while(1) {
    PROCESS_YIELD();
    
    if(movement_ready(ev, data)) {
      int mov = get_movement();
      blink_led(LEDS_GREEN);
      LOG_INFO("Read movement of %d Gs\n", mov);
      
      if(state != STATE_MOVING && mov > T) {
        LOG_INFO("Restart movement.\n");
        state = STATE_INIT_MOVE;
        ctimer_set(&acc_timer, MOVEMENT_PERIOD, init_movement_reading, NULL);
      }
      else if(state == STATE_MOVING && mov < T) {
        LOG_INFO("The user stopped moving.\n");
        state = STATE_INIT;
        ctimer_set(&acc_timer, G, init_movement_reading, NULL);
      }
      else if(state == STATE_MOVING) {
        ctimer_set(&acc_timer, MOVEMENT_PERIOD, init_movement_reading, NULL);
      }
      else {
        ctimer_set(&acc_timer, G, init_movement_reading, NULL);
      }
    }

    if (ev == PROCESS_EVENT_TIMER && data == &publish_periodic_timer) {
      state_machine();
    }
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

#if ENERGEST_CONF_ON == 1
PROCESS_THREAD(energest_process, ev, data)
{
  static struct etimer et;
  PROCESS_BEGIN();

  /* Delay 10 second */
  etimer_set(&et, ENERGEST_LOG_DELAY);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    etimer_reset(&et);

    /* Flush all energest times so we can read latest values */
    energest_flush();
    printf("Energest CPU: Active %lu LPM: %lu Deep LPM: %lu Total time: %lu seconds\n",
           (unsigned long)(energest_type_time(ENERGEST_TYPE_CPU) / ENERGEST_SECOND),
           (unsigned long)(energest_type_time(ENERGEST_TYPE_LPM) / ENERGEST_SECOND),
           (unsigned long)(energest_type_time(ENERGEST_TYPE_DEEP_LPM) / ENERGEST_SECOND),
           (unsigned long)(ENERGEST_GET_TOTAL_TIME() / ENERGEST_SECOND));
    printf("Energest Radio: Listen: %lu Transmit: %lu seconds\n",
           (unsigned long)(energest_type_time(ENERGEST_TYPE_LISTEN) / ENERGEST_SECOND),
           (unsigned long)(energest_type_time(ENERGEST_TYPE_TRANSMIT) / ENERGEST_SECOND));
  }
  PROCESS_END();
}
#endif