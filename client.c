/**	
 * \file
 *         Client-side application for the person detection project
 * 
 * Authors: Marco Bacis, Daniele Cattaneo
 */

#include <stdio.h>
#include <string.h>
#include "contiki.h"
#include "mqtt.h"
#include "rpl.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/tcp-socket.h"
#include "net/ipv6/sicslowpan.h"
#include "dev/leds.h"
#include "sys/log.h"
#include "movement.h"
#include "energest-log.h"


#define LOG_MODULE "PD Client"
#define LOG_LEVEL LOG_LEVEL_INFO


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
  AUTOSTART_PROCESSES(&client_process, &energest_process);
#else
  AUTOSTART_PROCESSES(&client_process);
#endif


#define MQTT_MAX_TOPIC_LENGTH 64
#define MQTT_MAX_CONTENT_LENGTH 64


/* A timeout used when waiting for something to happen (e.g. to connect or to
 * disconnect) */
#define STATE_MACHINE_PERIODIC     (CLOCK_SECOND >> 1)

/* Connections and reconnections */
#define RETRY_FOREVER              0xFF
#define RECONNECT_INTERVAL         (CLOCK_SECOND * 2)


static struct timer connection_life;
static uint8_t connect_attempt;


/* A timeout used when waiting to connect to a network */
#define NET_CONNECT_PERIODIC        (CLOCK_SECOND >> 2)


/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE    16


static struct mqtt_connection conn;


static struct etimer publish_periodic_timer;
static uint16_t seq_nr_value = 0;


int ipaddr_sprintf(char *buf, uint8_t buf_len, const uip_ipaddr_t *addr)
{
  uint16_t a;
  uint8_t len = 0;
  int i, f;
  for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
    a = (addr->u8[i] << 8) + addr->u8[i + 1];
    if(a == 0 && f >= 0) {
      if(f++ == 0) {
        len += snprintf(&buf[len], buf_len - len, "::");
      }
    } else {
      if(f > 0) {
        f = -1;
      } else if(i > 0) {
        len += snprintf(&buf[len], buf_len - len, ":");
      }
      len += snprintf(&buf[len], buf_len - len, "%x", a);
    }
  }

  return len;
}


static char *pub_topic(void)
{
  static char topic[MQTT_MAX_TOPIC_LENGTH] = "";
  char *ptopic = topic;
  int left = MQTT_MAX_TOPIC_LENGTH;
  
  ptopic = stpncpy(ptopic, MQTT_PUBLISH_TOPIC_PREFIX, left);
  left -= ptopic - topic;
  
  rpl_dag_t *dag = &curr_instance.dag;
  int len = ipaddr_sprintf(ptopic, left, &dag->dag_id);
  left -= len;

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if (left <= 0) {
    return MQTT_PUBLISH_TOPIC_PREFIX "unkroot";
  }

  LOG_INFO("MQTT topic change is \"%s\"\n", topic);
  return topic;
}


static char *client_id(void)
{
  static char client_id[2*6+1] = "";
  if (client_id[0] != '\0')
    return client_id;
    
  snprintf(client_id, 2*6+1, "%02x%02x%02x%02x%02x%02x",
           linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
           linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
           linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);
  return client_id;
}


static void mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
  switch(event) {
    case MQTT_EVENT_CONNECTED: {
      LOG_INFO("Application has a MQTT connection!\n");
      timer_set(&connection_life, CONNECTION_STABLE_TIME);
      state = STATE_CONNECTED;
      break;
    }
    case MQTT_EVENT_DISCONNECTED: {
      LOG_INFO("MQTT Disconnect: reason %u\n", *((mqtt_event_t *)data));
      state = STATE_DISCONNECTED;
      process_poll(&client_process);
      break;
    }
    case MQTT_EVENT_PUBACK: {
      LOG_INFO("Publishing complete\n");
      break;
    }
    default:
      LOG_WARN("Application got a unhandled MQTT event: %i\n", event);
      break;
  }
}


static void publish(void)
{
  /* Publish MQTT topic */
  static char app_buffer[MQTT_MAX_CONTENT_LENGTH];
  int len;
  int remaining = MQTT_MAX_CONTENT_LENGTH;

  seq_nr_value++;

  len = snprintf(app_buffer, remaining, "{\"client_id\":\"%s\",\"seq_nr_value\":\"%d\"}", client_id(), seq_nr_value); 

  if(len < 0 || len >= remaining) {
    LOG_ERR("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
    return;
  }

  mqtt_status_t res = mqtt_publish(&conn, NULL, pub_topic(), (uint8_t *)app_buffer,
               len, MQTT_QOS_LEVEL_1, MQTT_RETAIN_OFF);

  if(res == MQTT_STATUS_OK)
    LOG_INFO("Publish sent out (length %d, max %d)!\nMessage = %s\n", len, MQTT_MAX_CONTENT_LENGTH, app_buffer);
  else
    LOG_ERR("Error in publishing... %d\n", res);
}


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
      mqtt_register(&conn, &client_process, client_id(), mqtt_event,
                    MAX_TCP_SEGMENT_SIZE);
      mqtt_set_username_password(&conn, "use-token-auth", MQTT_AUTH_TOKEN);
                                     
      /* _register() will set auto_reconnect. We don't want that. */
      conn.auto_reconnect = 0;
      connect_attempt = 1;

      state = STATE_REGISTERED;
      LOG_INFO("Init\n");

    case STATE_REGISTERED:
      if(uip_ds6_get_global(ADDR_PREFERRED) != NULL) {
        /* Registered and with a global IPv6 address, connect! */
        LOG_INFO("Joined network! Connect attempt %u\n", connect_attempt);
        mqtt_connect(&conn, MQTT_BROKER_IP_ADDR, MQTT_BROKER_PORT, K * 3);
        state = STATE_CONNECTING;
      }
      etimer_set(&publish_periodic_timer, NET_CONNECT_PERIODIC);
      return;
      break;

    case STATE_CONNECTING:
      blink_led(LEDS_RED);
      LOG_INFO("Connecting: retry %u...\n", connect_attempt);
      break;

    case STATE_CONNECTED:
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
        etimer_set(&publish_periodic_timer, K);

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


PROCESS_THREAD(client_process, ev, data)
{
  leds_init();
  
  PROCESS_BEGIN();
  log_set_level("main", LOG_LEVEL_DBG);
  log_set_level("tcpip", LOG_LEVEL_DBG);
  
  init_movement_reading(NULL);

  state = STATE_MOVING;

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

