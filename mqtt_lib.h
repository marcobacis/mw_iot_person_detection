//#ifndef _MQTT_WRAPPER_LIB_
//#define _MQTT_WRAPPER_LIB_


#include "contiki.h"
#include "mqtt.h"
#include "rpl.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/tcp-socket.h"
#include "net/ipv6/sicslowpan.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "sys/log.h"
#define LOG_MODULE "MQTT-DEMO"
#define LOG_LEVEL LOG_LEVEL_INFO

#include <string.h>
/*---------------------------------------------------------------------------*/
/*
 * Publish to a local MQTT broker (e.g. mosquitto) running on
 * the node that hosts your border router
 */
static const char *broker_ip = MQTT_BROKER_IP_ADDR;
#define DEFAULT_ORG_ID              "mqtt-demo"
/*---------------------------------------------------------------------------*/
/*
 * A timeout used when waiting for something to happen (e.g. to connect or to
 * disconnect)
 */
#define STATE_MACHINE_PERIODIC     (CLOCK_SECOND >> 1)
/*---------------------------------------------------------------------------*/
/* Provide visible feedback via LEDS during various states */
/* When connecting to broker */
#define CONNECTING_LED_DURATION    (CLOCK_SECOND >> 2)

/* Each time we try to publish */
#define PUBLISH_LED_ON_DURATION    (CLOCK_SECOND)
/*---------------------------------------------------------------------------*/
/* Connections and reconnections */
#define RETRY_FOREVER              0xFF
#define RECONNECT_INTERVAL         (CLOCK_SECOND * 2)
/*---------------------------------------------------------------------------*/
/*
 * Number of times to try reconnecting to the broker.
 * Can be a limited number (e.g. 3, 10 etc) or can be set to RETRY_FOREVER
 */
#define RECONNECT_ATTEMPTS         RETRY_FOREVER
#define CONNECTION_STABLE_TIME     (CLOCK_SECOND * 5)
static struct timer connection_life;
static uint8_t connect_attempt;
/*---------------------------------------------------------------------------*/
/* Various states */

/*---------------------------------------------------------------------------*/
#define CONFIG_ORG_ID_LEN        32
#define CONFIG_TYPE_ID_LEN       32
#define CONFIG_AUTH_TOKEN_LEN    32
#define CONFIG_CMD_TYPE_LEN       8
#define CONFIG_IP_ADDR_STR_LEN   64
/*---------------------------------------------------------------------------*/
/* A timeout used when waiting to connect to a network */
#define NET_CONNECT_PERIODIC        (CLOCK_SECOND >> 2)
#define NO_NET_LED_DURATION         (NET_CONNECT_PERIODIC >> 1)
/*---------------------------------------------------------------------------*/
/* Default configuration values */
#define DEFAULT_TYPE_ID             "cc26xx"
#define DEFAULT_AUTH_TOKEN          "AUTHZ"
#define DEFAULT_SUBSCRIBE_CMD_TYPE  "+"
#define DEFAULT_BROKER_PORT         1883
#define DEFAULT_PUBLISH_INTERVAL    (60 * CLOCK_SECOND)
#define DEFAULT_KEEP_ALIVE_TIMER    60
/**
 * \brief Data structure declaration for the MQTT client configuration
 */
typedef struct mqtt_client_config {
  char org_id[CONFIG_ORG_ID_LEN];
  char type_id[CONFIG_TYPE_ID_LEN];
  char auth_token[CONFIG_AUTH_TOKEN_LEN];
  char broker_ip[CONFIG_IP_ADDR_STR_LEN];
  char cmd_type[CONFIG_CMD_TYPE_LEN];
  clock_time_t pub_interval;
  uint16_t broker_port;
} mqtt_client_config_t;
/*---------------------------------------------------------------------------*/
/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE    32
/*---------------------------------------------------------------------------*/
/*
 * Buffers for Client ID and Topic.
 * Make sure they are large enough to hold the entire respective string
 *
 * We also need space for the null termination
 */
#define BUFFER_SIZE 64
static char client_id[BUFFER_SIZE];
static char pub_topic[BUFFER_SIZE];
static char sub_topic[BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
/*
 * The main MQTT buffers.
 * We will need to increase if we start publishing more data.
 */
#define APP_BUFFER_SIZE 512
static struct mqtt_connection conn;
static char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
static struct etimer publish_periodic_timer;
static char *buf_ptr;
static uint16_t seq_nr_value = 0;
/*---------------------------------------------------------------------------*/
static mqtt_client_config_t conf;
/*---------------------------------------------------------------------------*/
int
ipaddr_sprintf(char *buf, uint8_t buf_len, const uip_ipaddr_t *addr)
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
/*---------------------------------------------------------------------------*/
static void
mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
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
/*---------------------------------------------------------------------------*/
static int
construct_pub_topic(void)
{
  rpl_dag_t *dag = &curr_instance.dag;
  
  //Should represent the border-router address
  char rootid[BUFFER_SIZE];
  
  ipaddr_sprintf(rootid, BUFFER_SIZE, &dag->dag_id);
  
  int len = snprintf(pub_topic, BUFFER_SIZE, "%s%s", MQTT_PUBLISH_TOPIC_PREFIX, rootid);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_ERR("Pub topic: %d, buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}

/*---------------------------------------------------------------------------*/
static int
construct_client_id(void)
{
  int len = snprintf(client_id, BUFFER_SIZE, "d:%s:%s:%02x%02x%02x%02x%02x%02x",
                     conf.org_id, conf.type_id,
                     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                     linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
                     linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_INFO("Client ID: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
static void
update_mqtt_config(void)
{
  if(construct_client_id() == 0) {
    /* Fatal error. Client ID larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }

  if(construct_pub_topic() == 0) {
    /* Fatal error. Topic larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }

  /* Reset the counter */
  seq_nr_value = 0;

  /*
   * Schedule next timer event ASAP
   *
   * If we entered an error state then we won't do anything when it fires.
   *
   * Since the error at this stage is a config error, we will only exit this
   * error state if we get a new config.
   */
  etimer_set(&publish_periodic_timer, 0);

  return;
}

static void init_mqtt()
{
  /* If we have just been configured register MQTT connection */
  mqtt_register(&conn, &client_process, client_id, mqtt_event,
                MAX_TCP_SEGMENT_SIZE);

  mqtt_set_username_password(&conn, "use-token-auth",
                                 conf.auth_token);

  /* _register() will set auto_reconnect. We don't want that. */
  conn.auto_reconnect = 0;
  connect_attempt = 1;

  LOG_INFO("Init\n");
}

/*---------------------------------------------------------------------------*/
static void
init_mqtt_config()
{
  /* Populate configuration with default values */
  memset(&conf, 0, sizeof(mqtt_client_config_t));

  memcpy(conf.org_id, DEFAULT_ORG_ID, strlen(DEFAULT_ORG_ID));
  memcpy(conf.type_id, DEFAULT_TYPE_ID, strlen(DEFAULT_TYPE_ID));
  memcpy(conf.auth_token, DEFAULT_AUTH_TOKEN, strlen(DEFAULT_AUTH_TOKEN));
  memcpy(conf.broker_ip, broker_ip, strlen(broker_ip));
  memcpy(conf.cmd_type, DEFAULT_SUBSCRIBE_CMD_TYPE, 1);

  conf.broker_port = DEFAULT_BROKER_PORT;
  conf.pub_interval = DEFAULT_PUBLISH_INTERVAL;
}
/*---------------------------------------------------------------------------*/
static void
subscribe(void)
{
  mqtt_status_t status;

  status = mqtt_subscribe(&conn, NULL, sub_topic, MQTT_QOS_LEVEL_0);

  LOG_INFO("Subscribing\n");
  if(status == MQTT_STATUS_OUT_QUEUE_FULL) {
    LOG_INFO("Tried to subscribe but command queue was full!\n");
  }
}
/*---------------------------------------------------------------------------*/
static void
publish(void)
{
  /* Publish MQTT topic */
  int len;
  int remaining = APP_BUFFER_SIZE;

  seq_nr_value++;

  buf_ptr = app_buffer;

  len = snprintf(buf_ptr, remaining, "{\"id\":\"%s\"}", client_id); 

  if(len < 0 || len >= remaining) {
    LOG_ERR("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
    return;
  }

  mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer,
               strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);

  LOG_INFO("Publish sent out!\n");
}
/*---------------------------------------------------------------------------*/
static void
connect_to_broker(void)
{
  /* Connect to MQTT server */
  mqtt_connect(&conn, conf.broker_ip, conf.broker_port,
               conf.pub_interval * 3);

  state = STATE_CONNECTING;
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/

