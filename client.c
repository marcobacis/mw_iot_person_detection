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
#ifdef MAC_CONF_WITH_TSCH
#include "tsch.h"
#include "tsch-private.h"
#endif
#include "movement.h"
#include "energest-log.h"
#include "led-report.h"


#define LOG_MODULE "PD Client"
#define LOG_LEVEL LOG_LEVEL_DBG


static char mqtt_connected;
process_event_t mqtt_did_connect;
process_event_t mqtt_did_disconnect;

static char is_moving = 1;
process_event_t mvmt_state_change;


PROCESS(client_process, "mw-iot-person-detection main");
PROCESS(movement_monitor_process, "mw-iot-person-detection mqtt monitor");

#if ENERGEST_CONF_ON == 1
  AUTOSTART_PROCESSES(&client_process, &energest_process);
#else
  AUTOSTART_PROCESSES(&client_process);
#endif


#define MQTT_MAX_TOPIC_LENGTH 64
#define MQTT_MAX_CONTENT_LENGTH 256


/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE    16


static struct mqtt_connection conn;


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
  
  int radio_rssi = -1000;
  int radio_pwr = -1000;
  NETSTACK_RADIO.get_value(RADIO_PARAM_RSSI, &radio_rssi);
  NETSTACK_RADIO.get_value(RADIO_PARAM_TXPOWER, &radio_pwr);

  len = snprintf(app_buffer, remaining, 
    "{"
      "\"client_id\":\"%s\","
      "\"seq_nr_value\":%d,"
      "\"last_accel\":[%d, %d, %d],"
      "\"curr_radio_rssi\":%d,"
      "\"curr_radio_power_dbm\":%d"
    "}", 
    client_id(), 
    seq_nr_value,
    last_acc[LAST_ACC_X], last_acc[LAST_ACC_Y], last_acc[LAST_ACC_Z],
    radio_rssi,
    radio_pwr); 

  if (len < 0 || len >= remaining) {
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


PROCESS_THREAD(movement_monitor_process, ev, data)
{
  static struct etimer acc_timer;
  static int old_movement = 0;
  
  PROCESS_BEGIN();
  
  is_moving = 1;
  mvmt_state_change = process_alloc_event();
  
  etimer_set(&acc_timer, SETUP_WAIT);
  
  while (1) {
    do {
      PROCESS_YIELD();
      LOG_DBG("wait timer ev=%x data=%p\n", ev, data);
    } while (ev != PROCESS_EVENT_TIMER && data != &acc_timer);
    
    init_movement_reading(NULL);
    do {
      PROCESS_YIELD();
      LOG_DBG("wait movement_ready ev=%x data=%p\n", ev, data);
    } while (!movement_ready(ev, data));
    
    clock_time_t next_wake;
    
    #if !DISABLE_MOVEMENT_SLEEP
    int raw_mov = get_movement();
    int mov = ABS(raw_mov - GRAVITY*GRAVITY);
    set_led_pattern(LEDS_GREEN, 0b1, 0);
    LOG_INFO("is_moving = %d, read movement of %d Gs\n", is_moving, mov);
    
    int moving_rn = mov >= T_MOD || ABS(mov - old_movement) >= T_DMOD;
    old_movement = mov;
    #else
    int moving_rn = 0;
    #endif
    
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
      next_wake = MOVEMENT_PERIOD;
      
    }
    etimer_set(&acc_timer, next_wake);
  }
  
  PROCESS_END();
}


int rpl_is_reachable_2(void)
{
  #ifdef CONTIKI_TARGET_NATIVE
  return 1;
  #else
  return rpl_is_reachable();
  #endif
}


PROCESS_THREAD(client_process, ev, data)
{
  static int mqtt_fake_disconnect = 0;
  static struct etimer timer;
  static enum {
    MQTT_STATE_IDLE,
    MQTT_STATE_RADIO_ON,
    MQTT_STATE_WAIT_IP,
    MQTT_STATE_CONNECT_MQTT,
    MQTT_STATE_WAIT_MQTT,
    MQTT_STATE_CONNECTED_PUBLISH,
    MQTT_STATE_CONNECTED_WAIT_PUBLISH,
    MQTT_STATE_DISCONNECT,
    MQTT_STATE_DISCONNECT_2,
    MQTT_STATE_DISCONNECT_3
  } mqtt_state = MQTT_STATE_IDLE;
  
  PROCESS_BEGIN();
  
  log_set_level("main", LOG_LEVEL_DBG);
  log_set_level("tcpip", LOG_LEVEL_DBG);
  log_set_level("rpl", LOG_LEVEL_DBG);
  log_set_level("mac", LOG_LEVEL_DBG);
  
  led_report_init();
  
  process_start(&movement_monitor_process, NULL);
  
  mqtt_connected = 0;
  mqtt_did_connect = process_alloc_event();
  mqtt_did_disconnect = process_alloc_event();

  etimer_set(&timer, STATE_MACHINE_PERIODIC);
  
  while (1) {
    /* Transitions */
    switch (mqtt_state) {
      case MQTT_STATE_IDLE:
        if (!is_moving) {
          mqtt_state = MQTT_STATE_RADIO_ON;
        }
        break;
        
      case MQTT_STATE_RADIO_ON:
        mqtt_state = MQTT_STATE_WAIT_IP;
        break;
        
      case MQTT_STATE_WAIT_IP: {
        int reachable = rpl_is_reachable_2();
        uip_ds6_addr_t *ip = uip_ds6_get_global(ADDR_PREFERRED);
        LOG_INFO("rpl is reachable = %d\n", reachable);
        LOG_INFO("uip_ds6_get_global(ADDR_PREFERRED) == %p\n", ip);
        if (ip != NULL && reachable) {
          mqtt_state = MQTT_STATE_CONNECT_MQTT;
        }
        break;
      }
        
      case MQTT_STATE_CONNECT_MQTT:
        mqtt_state = MQTT_STATE_WAIT_MQTT;

      case MQTT_STATE_WAIT_MQTT:
        if (ev == mqtt_did_connect) {
          LOG_INFO("Connected!\n");
          mqtt_fake_disconnect = 0;
          mqtt_state = MQTT_STATE_CONNECTED_PUBLISH;
        } else if (ev == mqtt_did_disconnect) {
          mqtt_state = MQTT_STATE_WAIT_IP;
        }
        break;
        
      case MQTT_STATE_CONNECTED_PUBLISH:
        mqtt_state = MQTT_STATE_CONNECTED_WAIT_PUBLISH;
        break;
        
      case MQTT_STATE_CONNECTED_WAIT_PUBLISH:
        if (ev == PROCESS_EVENT_TIMER && data == &timer) {
          mqtt_state = MQTT_STATE_CONNECTED_PUBLISH;
        }
        break;
        
      case MQTT_STATE_DISCONNECT_2:
      case MQTT_STATE_DISCONNECT:
        LOG_INFO("mqtt state = %d\n", conn.state);
        if (conn.state == MQTT_CONN_STATE_NOT_CONNECTED || mqtt_fake_disconnect)
          mqtt_state = MQTT_STATE_DISCONNECT_3;
        else
          mqtt_state = MQTT_STATE_DISCONNECT_2;
        break;
        
      case MQTT_STATE_DISCONNECT_3:
        mqtt_state = MQTT_STATE_IDLE;
        break;
    }
    
    if (is_moving && 
        mqtt_state != MQTT_STATE_IDLE && 
        mqtt_state != MQTT_STATE_DISCONNECT && 
        mqtt_state != MQTT_STATE_DISCONNECT_2 &&
        mqtt_state != MQTT_STATE_DISCONNECT_3) {
      LOG_INFO("Resetting MQTT state machine\n");
      mqtt_state = MQTT_STATE_DISCONNECT;
    }
    
    if (mqtt_state == MQTT_STATE_CONNECTED_PUBLISH || 
        mqtt_state == MQTT_STATE_CONNECTED_WAIT_PUBLISH) {
      if (ev == mqtt_did_disconnect || !rpl_is_reachable_2()) {
        LOG_INFO("MQTT disconnected...\n");
        mqtt_state = MQTT_STATE_WAIT_IP;
      }
    }
        
    /* States */
    switch(mqtt_state) {
      case MQTT_STATE_IDLE:
        break;

      case MQTT_STATE_RADIO_ON:
        LOG_INFO("Turning radio on\n");
        #ifdef MAC_CONF_WITH_TSCH
        /* TSCH-only: we know that probably we'll join a new network once we
         * are back up, so we disassociate manually */
        if (tsch_is_associated)
          tsch_disassociate();
        #endif
        NETSTACK_RADIO.on();
        NETSTACK_MAC.on();
        NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, CLIENT_RADIO_POWER_CONF);
        etimer_set(&timer, STATE_MACHINE_PERIODIC);
        break;
        
      case MQTT_STATE_WAIT_IP:
        set_led_pattern(LEDS_RED, 0b01, 0);
        LOG_INFO("Waiting IP address\n");
        etimer_set(&timer, STATE_MACHINE_PERIODIC);
        break;
        
      case MQTT_STATE_CONNECT_MQTT:
        set_led_pattern(LEDS_RED, 0b000000010001, 12);
        LOG_INFO("We have an IP; connection attempt to MQTT\n");
      
        /* If we have just been configured register MQTT connection */
        mqtt_register(&conn, &client_process, client_id(), mqtt_event,
                      MAX_TCP_SEGMENT_SIZE);
        mqtt_set_username_password(&conn, "use-token-auth", MQTT_AUTH_TOKEN);
        
        /* _register() will set auto_reconnect. We don't want that. */
        conn.auto_reconnect = 0;

        mqtt_connect(&conn, MQTT_BROKER_IP_ADDR, MQTT_BROKER_PORT, K * 3);
        break;
        
      case MQTT_STATE_WAIT_MQTT:
        LOG_INFO("Waiting MQTT connection\n");
        break;
        
      case MQTT_STATE_CONNECTED_PUBLISH:
        LOG_INFO("Should publish\n");
        if (mqtt_ready(&conn) && conn.out_buffer_sent) {
          set_led_pattern(LEDS_RED | LEDS_GREEN, 0b0101, 0);
          publish();
        } else {
          LOG_INFO("Still publishing... (MQTT state=%d, q=%u)\n", conn.state,
            conn.out_queue_full);
        }
        etimer_set(&timer, K);
        break;
        
      case MQTT_STATE_CONNECTED_WAIT_PUBLISH:
        break;

      case MQTT_STATE_DISCONNECT:
        set_led_pattern(LEDS_RED, 0b00010101, 0);
        LOG_INFO("Disconnecting MQTT\n");
        if (conn.state == MQTT_CONN_STATE_CONNECTED_TO_BROKER)
          mqtt_disconnect(&conn);
        else {
          /* When mqtt_disconnect is called on a non-completely-connected
           * MQTT connection object, the MQTT thread goes in an inconsistent
           * state, but we still have to do something about this */
          LOG_INFO("MQTT is not connected; skipping");
          mqtt_fake_disconnect = 1;
        }
        break;
        
      case MQTT_STATE_DISCONNECT_2:
        break;
        
      case MQTT_STATE_DISCONNECT_3:
        LOG_INFO("Shutting down radio\n");
        rpl_dag_leave();
        NETSTACK_MAC.off();
        NETSTACK_RADIO.off();
        /* TSCH-only issue: turning off MAC does not work for TSCH.
         * If TSCH is still in scanning mode, then we are *wasting power* and
         * we can do nothing about it because the TSCH stack is poorly written
         * and patching it to stop operating is not easy. */
        break;

      /* Should never happen */
      default:
        LOG_INFO("Default case: State=0x%02x\n", mqtt_state);
        break;
    }
    
    PROCESS_YIELD();
    LOG_DBG("MQTT thd woke, state=%d, ev=0x%02X, data=%p\n", mqtt_state, ev, data);
  }
  
  PROCESS_END();
}

