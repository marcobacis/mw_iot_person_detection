# mw_iot_person_detection
Iot project for the "Middleware Technologies for Distributed Systems" course at Polimi.


## HOWTO for cc2650 boards

Step 1: flash border router sw on launchpad boards and start router

```
cd ../contiki-ng-course/examples/rpl-border-router
make savetarget TARGET=srf06-cc26xx BOARD=launchpad/cc2650

make border-router.upload PORT=/dev/ttyACM0
make border-router.upload PORT=/dev/ttyACM2

make connect-router-ACM0
make connect-router-ACM2
```


make savetarget TARGET=srf06-cc26xx BOARD=sensortag/cc2650

make login PORT=/dev/ttyACM0


## HOWTO for cooja



## Tips

To monitor all messages being published over MQTT:

``mosquitto_sub -t '#'``

