# mw_iot_person_detection
Iot project for the "Middleware Technologies for Distributed Systems" course at Polimi.


## HOWTO for cc2650 boards

### Step 1: flash border router sw on launchpad boards and start router

```
cd ../contiki-ng-course/examples/rpl-border-router
make savetarget TARGET=srf06-cc26xx BOARD=launchpad/cc2650

make border-router.upload PORT=/dev/ttyACM0
make border-router.upload PORT=/dev/ttyACM2

make connect-router-ACM0 PREFIX=aaaa::1/64
make connect-router-ACM2 PREFIX=aaaa::1/64
```

### Step 2: flash sw on sensortag

```
make savetarget TARGET=srf06-cc26xx BOARD=sensortag/cc2650
make
[use Flash Programmer 2 on Windows to flash the sensortag with the .elf file]
make login PORT=/dev/ttyACM0
```

Note: to not disable JTAG, go to 
``contiki-ng-course/arch/cpu/cc26xx-cc13xx/lib/cc26xxware/startup_files/ccfg.c``, 
and under ``// Debug access settings`` un-comment all lines which set the 
registers to ``0xC5``. 

## HOWTO for cooja

pls don't

## Tips

To monitor all messages being published over MQTT:

``mosquitto_sub -t '#'``

