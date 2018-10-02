/** @file 
 * @brief Client-side application for the person detection project.
 *
 * Contains two main loops: one for checking if the device is currently
 * moving, and one which handles network operations.
 * 
 * @author Marco Bacis
 * @author Daniele Cattaneo */

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
#ifdef LOG_CONF_LEVEL_PD_CLIENT
#define LOG_LEVEL LOG_CONF_LEVEL_PD_CLIENT
#else
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#if MAC_CONF_WITH_TSCH
#define CSMA_MANUAL_DUTY_CYCLING  0
#else
#ifdef CSMA_CONF_MANUAL_DUTY_CYCLING
#define CSMA_MANUAL_DUTY_CYCLING  CSMA_CONF_MANUAL_DUTY_CYCLING
#else
#define CSMA_MANUAL_DUTY_CYCLING  1
#endif
#endif


process_event_t mqtt_did_connect;
process_event_t mqtt_did_disconnect;
process_event_t mqtt_did_publish;

static char is_moving = 1;
process_event_t mvmt_state_change;


PROCESS(client_process, "mw-iot-person-detection main");
PROCESS(movement_monitor_process, "mw-iot-person-detection mqtt monitor");

#if ENERGEST_CONF_ON == 1
  AUTOSTART_PROCESSES(&client_process, &energest_process);
#else
  AUTOSTART_PROCESSES(&client_process);
#endif


/** The length of the topic buffer. */
#define MQTT_MAX_TOPIC_LENGTH 64
/** The topic buffer. */
static char pub_topic_cache[MQTT_MAX_TOPIC_LENGTH] = "";

/** The length of the message buffer. */
#define MQTT_MAX_CONTENT_LENGTH 256

/** The current MQTT connection. */
static struct mqtt_connection conn;
static int mqtt_disconn_received;


/** Formats a IPv6 address into a string buffer.
 * @param buf     The output buffer. On return, the string in the buffer will
 *                always be null-terminated.
 * @param buf_len The size of the buffer (the resulting string will be long at 
 *                most buf_len-1 characters).
 * @param addr    The IPv6 address to be formatted.
 * @returns       The length of the string written into the buffer. */
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


/** Updates the string returned by pub_topic() to reflect the current network
 * configuration. */
static void update_pub_topic(void)
{
  char *ptopic = pub_topic_cache;
  int left = MQTT_MAX_TOPIC_LENGTH;
  
  ptopic = stpncpy(ptopic, MQTT_PUBLISH_TOPIC_PREFIX, left);
  left -= ptopic - pub_topic_cache;
  
  rpl_dag_t *dag = &curr_instance.dag;
  int len = ipaddr_sprintf(ptopic, left, &dag->dag_id);
  left -= len;

  if (left <= 0) {
    /* ensure that pub_topic_cache is properly terminated even on overflow */
    pub_topic_cache[MQTT_MAX_TOPIC_LENGTH-1] = '\0';
  }

  LOG_INFO("MQTT topic is now \"%s\"\n", pub_topic_cache);
}


/** Returns the string used as the MQTT topic.
 * The MQTT topic string consists of a concatenation of the value of 
 * MQTT_PUBLISH_TOPIC_PREFIX and the IPv6 address of the border router we
 * are currently connected to. Note that when the border router changes it is
 * necessary to call update_pub_topic() to update the string returned by
 * this function.
 * @returns The MQTT topic name.
 * @warning The returned string is a shared buffer which must not be reused. */
static char *pub_topic(void)
{
  if (pub_topic_cache[0] == '\0')
    update_pub_topic();
  return pub_topic_cache;
}


/** Returns an ID for the current client. 
 * @returns The client ID.
 * @note    Currently the ID returned is constructed fromthe link-local IPv6 
 *          address of the node */
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


/** Process an event from the current MQTT connection.
 * @param m     The MQTT connection the event is associated with.
 * @param event The event identifier.
 * @param data  The data associated with the event, if any. 
 * @warning     This function is meant to be called by the MQTT stack only.
 *              For more details see the Contiki MQTT documentation. */
static void mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
  switch(event) {
    case MQTT_EVENT_CONNECTED:
      LOG_INFO("Application has a MQTT connection!\n");
      process_post(&client_process, mqtt_did_connect, NULL);
      break;
    
    case MQTT_EVENT_DISCONNECTED: 
      LOG_INFO("MQTT Disconnect: reason %u\n", *((mqtt_event_t *)data));
      mqtt_disconn_received = 1;
      process_post(&client_process, mqtt_did_disconnect, NULL);
      break;
    
    case MQTT_EVENT_PUBACK:
      LOG_INFO("Publishing complete\n");
      #if CSMA_MANUAL_DUTY_CYCLING==1
      process_post(&client_process, mqtt_did_publish, NULL);
      #endif
      break;
    
    default:
      LOG_WARN("Application got a unhandled MQTT event: %i\n", event);
      break;
  }
}


/** Publishes a message over the current MQTT connection.
 * The message published contains the ID of the client and other useful
 * information for later analysis including the acceleration values measured,
 * the measured radio signal power, and the uptime of the node in seconds. */
static void publish(void)
{
  static uint16_t seq_nr_value = 0;
  static char app_buffer[MQTT_MAX_CONTENT_LENGTH];
  int len;

  seq_nr_value++;
  
  int radio_rssi = -1000;
  int radio_pwr = -1000;
  NETSTACK_RADIO.get_value(RADIO_PARAM_RSSI, &radio_rssi);
  NETSTACK_RADIO.get_value(RADIO_PARAM_TXPOWER, &radio_pwr);
  
  int clk = clock_time();

  len = snprintf(app_buffer, MQTT_MAX_CONTENT_LENGTH, 
    "{"
      "\"client_id\":\"%s\","
      "\"seq_nr_value\":%d,"
      "\"last_accel\":[%d, %d, %d],"
      "\"curr_radio_rssi\":%d,"
      "\"curr_radio_power_dbm\":%d,"
      "\"uptime\":%d.%02d"
    "}", 
    client_id(), 
    seq_nr_value,
    last_acc[LAST_ACC_X], last_acc[LAST_ACC_Y], last_acc[LAST_ACC_Z],
    radio_rssi,
    radio_pwr,
    clk / CLOCK_SECOND, (clk % CLOCK_SECOND) * 100 / CLOCK_SECOND); 

  if (len >= MQTT_MAX_CONTENT_LENGTH) {
    LOG_ERR("Buffer too short; MQTT message has been truncated!\n");
  }

  mqtt_status_t res = mqtt_publish(&conn, NULL, pub_topic(), (uint8_t *)app_buffer,
               len, MQTT_QOS_LEVEL_1, MQTT_RETAIN_OFF);

  if(res == MQTT_STATUS_OK) {
    LOG_INFO("Publish sent out (length %d, max %d)!\n", len, MQTT_MAX_CONTENT_LENGTH);
    LOG_INFO("Message = %s\n", app_buffer);
  } else {
    LOG_ERR("Error in publishing... %d\n", res);
  }
}


/** The process responsible for monitoring the movements of the device.
 *
 * This process periodically polls the accelerometer and determines if the
 * device has moved or not. It sends an event to client_process
 * each time the movement state has changed. */
PROCESS_THREAD(movement_monitor_process, ev, data)
{
  static struct etimer acc_timer;
  #if !DISABLE_MOVEMENT_SLEEP
  static int old_movement = 0;
  #endif
  
  PROCESS_BEGIN();
  
  is_moving = 1;
  mvmt_state_change = process_alloc_event();
  
  etimer_set(&acc_timer, SETUP_WAIT);
  
  while (1) {
    do {
      PROCESS_YIELD();
      LOG_DBG("wait timer ev=%x data=%p\n", ev, data);
    } while (!etimer_expired(&acc_timer));
    
    init_movement_reading();
    do {
      PROCESS_YIELD();
      LOG_DBG("wait movement_ready ev=%x data=%p\n", ev, data);
    } while (!movement_ready(ev, data));
    
    set_led_pattern(LEDS_GREEN, 0b1, 0);
    int raw_mov = get_movement();
    
    #if !DISABLE_MOVEMENT_SLEEP
    int mov = ABS(raw_mov - GRAVITY*GRAVITY);
    LOG_INFO("is_moving = %d, read movement of %d Gs\n", is_moving, mov);
    
    int moving_rn = mov >= T_MOD || ABS(mov - old_movement) >= T_DMOD;
    old_movement = mov;
    #else
    LOG_INFO("is_moving = 0, read raw movement of %d Gs\n", raw_mov);
    int moving_rn = 0;
    #endif
    
    clock_time_t next_wake;
    
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
      
    } else {
      #if PUBLISH_ON_MOVEMENT
      process_post(&client_process, mvmt_state_change, NULL);
      #endif
      next_wake = MOVEMENT_PERIOD;
      
    }
    etimer_reset_with_new_interval(&acc_timer, next_wake);
  }
  
  PROCESS_END();
}


/** Returns whether the RPL network is reachable, or 1 if there is no RPL
 * network. 
 * @returns The value returned by rpl_is_reachable() if using RPL, 
 *          1 otherwise. 
 * @note    This function exists to allow testing the software when 
 *          TARGET=native. */
int rpl_is_reachable_2(void)
{
  #ifdef CONTIKI_TARGET_NATIVE
  return 1;
  #else
  return rpl_is_reachable();
  #endif
}


/** The network management process.
 *
 * When the device is moving, as verified by movement_monitor_process,
 * this process disconnects any network connection -- if any -- and then
 * enters a wait state.
 *
 * When movement_monitor_process detects the device is not moving anymore,
 * this process does a best effort to connect to the first available 
 * network and starts periodically sending MQTT messages using the publish()
 * function. */
PROCESS_THREAD(client_process, ev, data)
{
  static int mqtt_fake_disconnect = 0;
  static struct etimer timer;
  static enum {
    /* Standard CSMA mode, TSCH mode:
     *   Waiting because the device is moving. 
     * Manual Duty Cycling CSMA mode:
     *   Waiting because the device is moving, or waiting for K seconds the time
     *   of the next MQTT message post. */
    MQTT_STATE_IDLE,
    /* The radio must be turned on. */
    MQTT_STATE_RADIO_ON,
    /* Waiting for the network connection to be established. */
    MQTT_STATE_WAIT_IP,
    /* The device must connect to the MQTT broker. */
    MQTT_STATE_CONNECT_MQTT,
    /* Waiting for the connection to the MQTT broker to be established. */
    MQTT_STATE_WAIT_MQTT,
    /* The device must publish on the MQTT topic. */
    MQTT_STATE_CONNECTED_PUBLISH,
    /* Standard CSMA mode, TSCH mode:
     *   The device is waiting for K seconds the time of the next MQTT message
     *   post.
     * Manual Duty Cycling CSMA mode:
     *   The device is waiting for the MQTT message to be fully delivered. */
    MQTT_STATE_CONNECTED_WAIT_PUBLISH,
    /* Disconnection Phase 1: Initiate MQTT teardown */
    MQTT_STATE_DISCONNECT,
    /* Disconnection Phase 2: Waiting for the MQTT connection to close */
    MQTT_STATE_DISCONNECT_2,
    /* Disconnection Phase 3: Turn off radios */
    MQTT_STATE_DISCONNECT_3
  } mqtt_state = MQTT_STATE_IDLE;
  
  PROCESS_BEGIN();
  
  log_set_level("main", LOG_LEVEL_DBG);
  log_set_level("tcpip", LOG_LEVEL_DBG);
  log_set_level("rpl", LOG_LEVEL_DBG);
  log_set_level("mac", LOG_LEVEL_DBG);
  
  led_report_init();
  
  process_start(&movement_monitor_process, NULL);
  
  mqtt_did_connect = process_alloc_event();
  mqtt_did_disconnect = process_alloc_event();
  mqtt_did_publish = process_alloc_event();
  
  /* Register MQTT connection
   * Registering MQTT multiple times causes memory corruption!! */
  mqtt_register(&conn, &client_process, client_id(), mqtt_event, MAX_TCP_SEGMENT_SIZE);
  mqtt_set_username_password(&conn, "use-token-auth", MQTT_AUTH_TOKEN);
  /* _register() will set auto_reconnect. We don't want that. */
  conn.auto_reconnect = 0;
  
  /* Turn off MAC and radio for consistency with initial state = moving */
  NETSTACK_MAC.off();
  NETSTACK_RADIO.off();

  etimer_set(&timer, STATE_MACHINE_PERIODIC);
  
  while (1) {
    /* Transitions.
     * mqtt_state is still set to the state that was executed in the previous
     * iteration */
    switch (mqtt_state) {
      case MQTT_STATE_IDLE:
        #if CSMA_MANUAL_DUTY_CYCLING==1 && PUBLISH_ON_MOVEMENT==0
        if (!is_moving && etimer_expired(&timer)) {
          mqtt_state = MQTT_STATE_RADIO_ON;
        } 
        #else
        if (!is_moving) {
          mqtt_state = MQTT_STATE_RADIO_ON;
        }
        #endif
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
        if (ev == mqtt_did_connect || mqtt_connected(&conn)) {
          LOG_INFO("Connected!\n");
          mqtt_fake_disconnect = 0;
          mqtt_state = MQTT_STATE_CONNECTED_PUBLISH;
          update_pub_topic();
        } else if (mqtt_disconn_received) {
          mqtt_state = MQTT_STATE_WAIT_IP;
        }
        break;
        
      case MQTT_STATE_CONNECTED_PUBLISH:
        mqtt_state = MQTT_STATE_CONNECTED_WAIT_PUBLISH;
        break;
        
      case MQTT_STATE_CONNECTED_WAIT_PUBLISH:
        #if CSMA_MANUAL_DUTY_CYCLING==1
        if (ev == mqtt_did_publish) {
          mqtt_state = MQTT_STATE_DISCONNECT;
        }
        #else
        #if PUBLISH_ON_MOVEMENT==0
        if (ev == PROCESS_EVENT_TIMER && data == &timer) {
          mqtt_state = MQTT_STATE_CONNECTED_PUBLISH;
        }
        #else
        if (ev == mvmt_state_change) {
          mqtt_state = MQTT_STATE_CONNECTED_PUBLISH;
        }
        #endif
        #endif
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
    
    /* Global triggers */
    if (is_moving && 
        mqtt_state != MQTT_STATE_IDLE && 
        mqtt_state != MQTT_STATE_DISCONNECT && 
        mqtt_state != MQTT_STATE_DISCONNECT_2 &&
        mqtt_state != MQTT_STATE_DISCONNECT_3) {
      /* As soon as movement is detected, abort all connections (even those
       * that could not be established */
      LOG_INFO("Resetting MQTT state machine\n");
      mqtt_state = MQTT_STATE_DISCONNECT;
    }
    
    if (mqtt_state == MQTT_STATE_CONNECTED_PUBLISH || 
        mqtt_state == MQTT_STATE_CONNECTED_WAIT_PUBLISH) {
      if (mqtt_disconn_received || !rpl_is_reachable_2()) {
        LOG_INFO("MQTT disconnected...\n");
        mqtt_state = MQTT_STATE_WAIT_IP;
      }
    }
        
    /* States */
    switch(mqtt_state) {
      case MQTT_STATE_IDLE:
        break;

      case MQTT_STATE_RADIO_ON:
        set_led_pattern(LEDS_RED, 0b1, 20);
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
        LOG_INFO("Waiting IP address\n");
        etimer_set(&timer, STATE_MACHINE_PERIODIC);
        break;
        
      case MQTT_STATE_CONNECT_MQTT:
        set_led_pattern(LEDS_RED, 0b000000010001, 12);
        LOG_INFO("We have an IP; connection attempt to MQTT\n");

        mqtt_disconn_received = 0;
        mqtt_status_t stat;
        stat = mqtt_connect(&conn, MQTT_BROKER_IP_ADDR, MQTT_BROKER_PORT, K * 3);
        if (stat != MQTT_STATUS_OK)
          mqtt_disconn_received = 1;
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
        #if CSMA_MANUAL_DUTY_CYCLING==0 && PUBLISH_ON_MOVEMENT==0
        etimer_set(&timer, K);
        #endif
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
        #if CSMA_MANUAL_DUTY_CYCLING == 1
        /* setup a wake for publishing again instead of waiting indefinitely
         * The state machine will automatically reconnect and publish because  
         * the MQTT_STATE_INIT transition trigger always checks is_moving */
        etimer_set(&timer, K);
        #endif
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

