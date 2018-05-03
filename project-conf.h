/*---------------------------------------------------------------------------*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
/*---------------------------------------------------------------------------*/
/* Disable PROP_MODE */
#define CC13XX_CONF_PROP_MODE 0


// Project-related defines

//MQTT topic prefix (in addition to the border-router ID)
#define RPL_PREFIX "people"

//Threshold to identify movement
#define T 10

//Period to send mqtt messages when connected & not moving (seconds)
#define K 10

//Time to wait before checking if the person is moving (seconds)
#define G 4


/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
/*---------------------------------------------------------------------------*/
