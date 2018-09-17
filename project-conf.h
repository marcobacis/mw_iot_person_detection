/*---------------------------------------------------------------------------*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
/*---------------------------------------------------------------------------*/
/* Disable PROP_MODE */
#define CC13XX_CONF_PROP_MODE 0

/* Enables energest for energy consumption time logging (only software) */
//#define ENERGEST_CONF_ON 1
#define ENERGEST_CONF_CURRENT_TIME clock_time
#define ENERGEST_CONF_TIME_T clock_time_t
#define ENERGEST_CONF_SECOND CLOCK_SECOND
#define ENERGEST_LOG_DELAY 60 * CLOCK_SECOND

// Enables TCP
#define UIP_CONF_TCP 1

#define RPL_CONF_LEAF_ONLY 1

// Project-related defines

//Threshold to identify movement
#define T 10

//Movement reading period
#define MOVEMENT_PERIOD (10 * CLOCK_SECOND)

#define MOVEMENT_FILE "acceleration.h"

//Period to send mqtt messages when connected & not moving (seconds)
#define K (10 * CLOCK_SECOND)

//Time to wait before checking if the person is moving (seconds)
#define G (20 * CLOCK_SECOND)

//MQTT-specific defines

#define MQTT_PUBLISH_TOPIC_PREFIX   "iot/position/"
#define MQTT_DEFAULT_BROKER_IP_ADDR "aaaa::1"

#define RECONNECT_ATTEMPTS         2
#define CONNECTION_STABLE_TIME     (CLOCK_SECOND * 5)

// Logging defines
/*#define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_6LOWPAN                     LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_DBG
*/
/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
/*---------------------------------------------------------------------------*/
