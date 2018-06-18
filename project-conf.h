/*---------------------------------------------------------------------------*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
/*---------------------------------------------------------------------------*/
/* Disable PROP_MODE */
#define CC13XX_CONF_PROP_MODE 0

// Enables TCP
#define UIP_CONF_TCP 1

// Project-related defines

//Threshold to identify movement
#define T 10

//Movement reading period
#define MOVEMENT_PERIOD (4 * CLOCK_SECOND)

#define MOVEMENT_FILE "acceleration.h"

//Period to send mqtt messages when connected & not moving (seconds)
#define K (5 * CLOCK_SECOND)

//Time to wait before checking if the person is moving (seconds)
#define G (10 * CLOCK_SECOND)

//MQTT-specific defines

#define MQTT_PUBLISH_TOPIC_PREFIX   "iot/position/"
#define MQTT_BROKER_IP_ADDR "aaaa::1"

#define MQTT_DEMO_STATUS_LED  LEDS_GREEN
#define MQTT_DEMO_TRIGGER_LED LEDS_RED
#define MQTT_DEMO_PUBLISH_TRIGGER &button_left_sensor

#define RECONNECT_ATTEMPTS         RETRY_FOREVER
#define CONNECTION_STABLE_TIME     (CLOCK_SECOND * 5)


/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
/*---------------------------------------------------------------------------*/
