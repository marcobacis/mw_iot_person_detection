/* Configuration file of the Middleware Person Detection Project
 * 
 * @author Marco Bacis
 * @author Daniele Cattaneo */
 
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_


/*
 * NETWORK OPTIONS
 */

/* Disable PROP_MODE (Low-GHz IEEE 802.15.4, only for CC13xx boards) */
#define CC13XX_CONF_PROP_MODE       0

/* Enable TCP */
#define UIP_CONF_TCP                1

/* If this node must stay a leaf or not */
#define RPL_CONF_LEAF_ONLY          1

/* MQTT configuration */
#define MQTT_PUBLISH_TOPIC_PREFIX   "iot/position/"
#define MQTT_BROKER_IP_ADDR         "aaaa::1"
#define MQTT_BROKER_PORT            1883
#define MQTT_TYPE_ID                "cc26xx"
#define MQTT_AUTH_TOKEN             "AUTHZ"
#define MQTT_SUBSCRIBE_CMD_TYPE     "+"

/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE        16

/* A timeout used when waiting for something to happen (e.g. to connect or to
 * disconnect) */
#define STATE_MACHINE_PERIODIC     (CLOCK_SECOND)

/* RPL configuration
 * We want short RPL lifetime and probing interval because we expect network
 * disconnections and reconnections to be frequent */
#define RPL_CONF_DEFAULT_LIFETIME_UNIT      1
#define RPL_CONF_DEFAULT_LIFETIME           60
#define RPL_CONF_PROBING_INTERVAL           (2 * CLOCK_SECOND)
#define RPL_CONF_DELAY_BEFORE_LEAVING       (15 * CLOCK_SECOND)
#define RPL_CONF_DIS_INTERVAL               (15 * CLOCK_SECOND)
#define RPL_CONF_NOPATH_REMOVAL_DELAY       60
#define RPL_CONF_DAO_MAX_RETRANSMISSIONS    2
#define RPL_CONF_DAO_RETRANSMISSION_TIMEOUT (2 * CLOCK_SECOND)

/* Radio power setting in dBm
 * Default is 5 dBm */
#define CLIENT_RADIO_POWER_CONF             (0)

/* Period between each channel hop when establishing a TSCH. 
 * Tweak if connection is too slow, it could make a difference. */
#define TSCH_CONF_CHANNEL_SCAN_DURATION     (CLOCK_SECOND / 4)

/* Enable manual duty cycling in CSMA mode. When manual duty cycling is 
 * enabled, the radio is kept on only for the duration of time needed to
 * connect to the MQTT broker and send a single message, then it is turned
 * off immediately.
 * Warning: to avoid excessive power consumption for small values of K, when 
 * manual duty cycling is on, K is effectively increased by the amount of
 * time needed to connect to the RPL network after the radio is turned on
 * (usually around 10 to 15 seconds with default settings). */
#define CSMA_CONF_MANUAL_DUTY_CYCLING       1


/*
 * ENERGEST CONFIGURATION
 */
 
#ifndef ENERGEST_CONF_ON
#define ENERGEST_CONF_ON 1
#endif

#define ENERGEST_CONF_CURRENT_TIME  clock_time
#define ENERGEST_CONF_TIME_T        clock_time_t
#define ENERGEST_CONF_SECOND        CLOCK_SECOND
#define ENERGEST_LOG_DELAY          (60 * CLOCK_SECOND)


/*
 * MOVEMENT CHECKING OPTIONS
 */
 
/* The delay between the power on of the device and actual start of operation */
#define SETUP_WAIT  (CLOCK_SECOND * 5)
 
/* If set to 1, disables sleeping when movement is detected. Useful for
 * collecting sensor data. */
#define DISABLE_MOVEMENT_SLEEP 0

/* Acceleration thresholds */
#define T_MOD   1000
#define T_DMOD  500

/* Movement reading period */
#ifdef CONTIKI_TARGET_NATIVE
#define MOVEMENT_PERIOD (CLOCK_SECOND)
#else
#define MOVEMENT_PERIOD (3 * CLOCK_SECOND)
#endif

/* Acceleration script file to use for plaforms without an accelerometer */
#define MOVEMENT_FILE "acceleration.h"

/* Period of periodic MQTT messages sent when connected & not moving */
#define K (CLOCK_SECOND * 10)

/* Time to wait before resuming accelerometer polling after the device has
 * just stopped moving */
#ifdef CONTIKI_TARGET_NATIVE
#define G (CLOCK_SECOND / 2)
#else
#define G (50 * CLOCK_SECOND)
#endif


/*
 * LOGGING
 */

/* Set to zero to disable blinkenlights. When enabled, leds blink with 
 * different patterns depending on the internal state of the software. */ 
#define ENABLE_LEDS       1

/* Log level for the main person detection software. */
#define LOG_CONF_LEVEL_PD_CLIENT                   LOG_LEVEL_ERR
/* Log level for the led-report module. */
#define LOG_CONF_LEVEL_LED_REPORT                  LOG_LEVEL_ERR
/* Log level for the energest-log module. */
#define LOG_CONF_LEVEL_ENERGEST_LOG                LOG_LEVEL_DBG
/* Log level for the movement module. */
#define LOG_CONF_LEVEL_MOVEMENT                    LOG_LEVEL_ERR

/* Log level for useful Contiki modules */
#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_ERR
#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_ERR
#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_ERR


#endif /* PROJECT_CONF_H_ */
