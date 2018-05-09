/*---------------------------------------------------------------------------*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
/*---------------------------------------------------------------------------*/
/* Disable PROP_MODE */
#define CC13XX_CONF_PROP_MODE 0


// Project-related defines

//Threshold to identify movement
#define T 10

//Movement reading period
#define MOVEMENT_PERIOD (2*CLOCK_SECOND)

//Period to send mqtt messages when connected & not moving (seconds)
#define K 10

//Time to wait before checking if the person is moving (seconds)
#define G 4

//MQTT-specific defines

#define MQTT_PUBLISH_TOPIC_PREFIX   "iot/position/"
#define MQTT_BROKER_IP_ADDR "fd00::1"

#define RECONNECT_ATTEMPTS         RETRY_FOREVER
#define CONNECTION_STABLE_TIME     (CLOCK_SECOND * 5)


/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
/*---------------------------------------------------------------------------*/
