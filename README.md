# mw\_iot\_person\_detection

Iot project for the "Middleware Technologies for Distributed Systems" course at Polimi.


## HOWTO for cc2650 boards

### Step 1: flash border router software on Launchpad boards and start router

```
cd ../contiki-ng-course/examples/rpl-border-router
make savetarget TARGET=srf06-cc26xx BOARD=launchpad/cc2650

make border-router.upload PORT=/dev/ttyACM0
make border-router.upload PORT=/dev/ttyACM2

make connect-router-ACM0 PREFIX=aaaa::1/64
make connect-router-ACM2 PREFIX=aaaa::1/64
```

(the device names `/dev/ttyACMx` can change)

### Step 2: flash client software on the SensorTag

```
cd mw_iot_person_detection
make savetarget TARGET=srf06-cc26xx BOARD=sensortag/cc2650
make
[use Flash Programmer 2 on Windows to flash the sensortag with the .elf file]
make login PORT=/dev/ttyACM0
```

Note: to not disable JTAG, go to 
``contiki-ng-course/arch/cpu/cc26xx-cc13xx/lib/cc26xxware/startup_files/ccfg.c``, 
and under ``// Debug access settings`` un-comment all lines which set the 
registers to ``0xC5``. 

### Step 3: Profit

The sensortag is now ready to use and will connect to the border routers automatically.

It is possible to view debug output by connecting to the sensortag via ACM RS232 by running:

```
make login PORT=/dev/ttyACM0
```

(the device name `/dev/ttyACM0` can change)


## HOWTO locally on macOS

### Step 1: Install tuntaposx

Download the latest version of tuntaposx from [the official website](http://tuntaposx.sourceforge.net/download.xhtml) and install the pkg.

You can check if the installation succeeded by looking for the existence of the `/dev/tun0` device and by running `kextstat`.

### Step 2: compile for native

```
cd mw_iot_person_detection
make savetarget TARGET=native
make WERROR=0
```

### Step 3: run the software

```
sudo ./client.native
```

Then, on a different tty:

```
sudo ifconfig tun0 inet6 aaaa::1/64
mosquitto -v
```

Warning: the setup of the `tun0` interface through ifconfig must be repeated every time and it must be done after `./client.native` has been started; because while the `/dev/tun0` device always exists, it is registered as a network interface only after it is opened by a client.

## HOWTO for cooja

pls don't

## Tips

To monitor all messages being published over MQTT:

``mosquitto_sub -t '#'``

