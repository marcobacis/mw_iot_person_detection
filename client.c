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
#include "led-report.h"


#define LOG_MODULE "PD Client"
#define LOG_LEVEL LOG_LEVEL_INFO


static char mqtt_connected;
process_event_t mqtt_did_connect;
process_event_t mqtt_did_disconnect;

static char is_moving = 1;
process_event_t mvmt_state_change;


PROCESS(client_process, "mw-iot-person-detection main");
PROCESS(mqtt_monitor_process, "mw-iot-person-detection mqtt monitor");

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


/* A timeout used when waiting to connect to a network */
#define NET_CONNECT_PERIODIC        (CLOCK_SECOND >> 2)


/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE    16


static struct mqtt_connection conn;


static struct etimer mqtt_publish_timer;
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
    case MQTT_EVENT_CONNECTED:
      LOG_INFO("Application has a MQTT connection!\n");
      timer_set(&connection_life, CONNECTION_STABLE_TIME);
      process_post(&client_process, mqtt_did_connect, NULL);
      break;
    
    case MQTT_EVENT_DISCONNECTED: 
      LOG_INFO("MQTT Disconnect: reason %u\n", *((mqtt_event_t *)data));
      process_post(&client_process, mqtt_did_disconnect, NULL);
      break;
    
    case MQTT_EVENT_PUBACK:
      LOG_INFO("Publishing complete\n");
      break;
    
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


PROCESS_THREAD(mqtt_monitor_process, ev, data)
{
  static struct etimer acc_timer;
  
  PROCESS_BEGIN();
  
  is_moving = 1;
  mvmt_state_change = process_alloc_event();
  init_movement_reading(NULL);
  
  while (1) {
    PROCESS_YIELD();
    clock_time_t next_wake;
    
    if (movement_ready(ev, data)) {
      int mov = get_movement();
      set_led_pattern(LEDS_GREEN, 0b1, 0);
      LOG_INFO("Read movement of %d Gs\n", mov);
      
      char moving_rn = mov > T_HIGH || mov < T_LOW;
      
      if(!is_moving && moving_rn) {
        LOG_INFO("User started moving.\n");
        is_moving = 1;
        process_post(&client_process, mvmt_state_change, NULL);
        next_wake = MOVEMENT_PERIOD;
        
      } else if(is_moving && !moving_rn) {
        LOG_INFO("User stopped moving.\n");
        is_moving = 0;
        process_post(&client_process, mvmt_state_change, NULL);
        next_wake = G;
        
      } else if (is_moving) {
        next_wake = MOVEMENT_PERIOD;
        
      } else {
        next_wake = G;
        
      }
      etimer_set(&acc_timer, next_wake);
      
      do {
        PROCESS_YIELD();
      } while (ev != PROCESS_EVENT_TIMER && data != &acc_timer);
      init_movement_reading(NULL);
    }
  }
  
  PROCESS_END();
}


PROCESS_THREAD(client_process, ev, data)
{
  static int connect_attempt = 0;
  static enum {
    MQTT_STATE_IDLE,
    MQTT_STATE_INIT,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_RETRY_CONNECTION,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_DISCONNECT
  } mqtt_state = MQTT_STATE_INIT;
  
  PROCESS_BEGIN();
  
  log_set_level("main", LOG_LEVEL_DBG);
  log_set_level("tcpip", LOG_LEVEL_DBG);
  
  led_report_init();
  process_start(&mqtt_monitor_process, NULL);
  
  mqtt_connected = 0;
  mqtt_did_connect = process_alloc_event();
  mqtt_did_disconnect = process_alloc_event();

  etimer_set(&mqtt_publish_timer, STATE_MACHINE_PERIODIC);
  
  while (1) {
    static char force_wait = 0;
    clock_time_t next_wake = 0;
    
    /* Externally triggered transitions */
    if (mqtt_state == MQTT_STATE_IDLE && !is_moving) {
      mqtt_state = MQTT_STATE_INIT;
      
    } else if (mqtt_state == MQTT_STATE_CONNECTING && ev == mqtt_did_connect) {
      mqtt_state = MQTT_STATE_CONNECTED;
      
    } else if (mqtt_state == MQTT_STATE_CONNECTING && ev == mqtt_did_disconnect) {
      mqtt_state = MQTT_STATE_RETRY_CONNECTION;
      
    } else if (mqtt_state == MQTT_STATE_CONNECTED && is_moving) {
      mqtt_state = MQTT_STATE_DISCONNECT;
      
    } else if (mqtt_state == MQTT_STATE_CONNECTED && ev == mqtt_did_disconnect) {
      mqtt_state = MQTT_STATE_IDLE;
    }
    
    LOG_DBG("MQTT thread exec state %d\n", mqtt_state);
    
    switch(mqtt_state) {
      /* Disconnected; waiting for connection-triggering event */
      case MQTT_STATE_IDLE:
        set_led_pattern(LEDS_RED, 0, 0);
        break;

      /* Connection-triggering event received; try connecting */
      case MQTT_STATE_INIT:
        LOG_INFO("MQTT initialization\n");
        
        /* If we have just been configured register MQTT connection */
        mqtt_register(&conn, &client_process, client_id(), mqtt_event,
                      MAX_TCP_SEGMENT_SIZE);
        mqtt_set_username_password(&conn, "use-token-auth", MQTT_AUTH_TOKEN);
        
        /* _register() will set auto_reconnect. We don't want that. */
        conn.auto_reconnect = 0;
        connect_attempt = 1;

        if (uip_ds6_get_global(ADDR_PREFERRED) != NULL) {
          /* Registered and with a global IPv6 address, connect! */
          LOG_INFO("Connect attempt %u\n", connect_attempt);
          mqtt_connect(&conn, MQTT_BROKER_IP_ADDR, MQTT_BROKER_PORT, K * 3);
          mqtt_state = MQTT_STATE_CONNECTING;
          set_led_pattern(LEDS_RED, 0b000000010001, 12);
          next_wake = NET_CONNECT_PERIODIC;
        } else {
          LOG_INFO("No IP address; ");
          if (is_moving) {
            LOG_INFO("we are moving; stop trying\n");
            mqtt_state = MQTT_STATE_IDLE;
          } else {
            LOG_INFO("retrying in a short while\n");
            next_wake = RECONNECT_INTERVAL;
          }
          set_led_pattern(LEDS_RED, 0b01, 0);
        }
        break;

      /* Waiting for connection established MQTT event */
      case MQTT_STATE_CONNECTING:
        LOG_INFO("Still connecting: retry %u...\n", connect_attempt);
        break;
        
      /* Disconnected after attempting an MQTT connection */
      case MQTT_STATE_RETRY_CONNECTION:
        set_led_pattern(LEDS_RED, 0b00010101, 0);
        LOG_INFO("Disconnected\n");
        
        if (is_moving) {
          mqtt_state = MQTT_STATE_IDLE;
          LOG_INFO("We did move; stop trying to connect");
          
        } else  {
          /* Disconnect and backoff */
          mqtt_disconnect(&conn);
          connect_attempt++;
          LOG_INFO("Disconnected: attempt %u in %d ticks\n", connect_attempt, RECONNECT_INTERVAL);

          next_wake = RECONNECT_INTERVAL;
          force_wait = 1;
          mqtt_state = MQTT_STATE_INIT;
        }
        break;

      /* Connected successfully */
      case MQTT_STATE_CONNECTED:
        /* If the timer expired, the connection is stable. */
        if (timer_expired(&connection_life)) {
          connect_attempt = 0;
        }

        if (ev != PROCESS_EVENT_TIMER && ev != mqtt_did_connect)
          break;
          
        LOG_INFO("Should publish\n");
        if(mqtt_ready(&conn) && conn.out_buffer_sent) {
          /* Connected. Publish */
          set_led_pattern(LEDS_RED | LEDS_GREEN, 0b0101, 0);
          publish();
          next_wake = K;
          
        } else {
          /* Our publish timer fired, but some MQTT packet is already in flight
           * (either not sent at all, or sent but not fully ACKd).
           *
           * This can mean that we have lost connectivity to our broker or that
           * simply there is some network delay. In both cases, we refuse to
           * trigger a new message and we wait for TCP to either ACK the entire
           * packet after retries, or to timeout and notify us. */
          set_led_pattern(LEDS_RED, 0b0101, 0);
          if(!mqtt_connected(&conn) || !rpl_is_reachable()) {
            LOG_INFO("mqtt disconnected...\n");
            mqtt_state = MQTT_STATE_IDLE;
          } else {
            LOG_INFO("Still publishing... (MQTT state=%d, q=%u)\n", conn.state,
              conn.out_queue_full);
            next_wake = K;
          }
        }
        break;
        
      case MQTT_STATE_DISCONNECT:
        set_led_pattern(LEDS_RED, 0b0101, 0);
        LOG_INFO("Disconnecting MQTT\n");
        mqtt_disconnect(&conn);
        mqtt_state = MQTT_STATE_IDLE;
        break;

      /* Should never happen */
      default:
        LOG_INFO("Default case: State=0x%02x\n", mqtt_state);
        break;
    }

    /* Reschedule ourselves */
    if (next_wake > 0) {
      etimer_set(&mqtt_publish_timer, next_wake);
      LOG_DBG("MQTT thd next wake = %ld\n", next_wake);
    } else {
      force_wait = 0;
    }
    do {
      PROCESS_YIELD();
      LOG_DBG("MQTT thd woke, force_wait = %d, ev = 0x%02X, data = %p\n", force_wait, ev, data);
    } while (force_wait && ev != PROCESS_EVENT_TIMER);
    force_wait = 0;
  }
  
  PROCESS_END();
}

