# Middleware IoT Person Detection

IoT project for the "Middleware Technologies for Distributed Systems" course at
Polimi, consisting of an IPv6-based presence detection system using MQTT.


## Abstract

People roaming in a given area carry wireless sensors able to sense
acceleration.

Every relevant location in the environment, e.g., every room, is equipped 
with a static wireless device, which acts as an IPv6 border router and is 
wired to a standard host running the IPv6 stack, e.g., through a USB connection.

When the user is moving, the device saves as much energy as possible by turning
off the radio and the sensors during idle time.

Once the mobile device detects the user has stopped moving, it connects to a 
nearby border router (if one is reachable) and periodically publishes an MQTT 
message every K time units.


## HOWTO for cc2650 boards

### Step 1: flash border router software on Launchpad boards and start router

```
cd ../contiki-ng-course/examples/rpl-border-router
make savetarget TARGET=srf06-cc26xx BOARD=launchpad/cc2650
```

Flash each board (assuming the RS232 interface is connected to `/dev/ttyACMx`):
```
make border-router.upload PORT=/dev/ttyACMx
```

Reset the board and then connect it to the external network. On Linux:
```
make connect-router-ACMx PREFIX=aaaa::1/64
```

On macOS, first install
[tuntaposx](http://tuntaposx.sourceforge.net/download.xhtml), then:
```
../../tools/tunslip6 -L -v2 -t tun0 -s ttyACMx aaaa::1/64
```

The device names `/dev/ttyACMx` can change. If you plan to configure the client
for TSCH, compile `rpl-border-router-tsch` instead.

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

To configure for TSCH, add the `MAKE_MAC=MAKE_MAC_TSCH` parameter when invoking
`make`.

### Step 3: Ready to Go!

The sensortag is now ready to use and will connect to the border routers 
automatically.

Start an MQTT broker on the border router host machines to let the software
send MQTT messages when the user has stopped moving.

It is possible to view debug output by connecting to the sensortag via 
ACM RS232 by running:

```
make login PORT=/dev/ttyACM0
```

(the device name `/dev/ttyACM0` can change)


## HOWTO locally on macOS

### Step 1: Install tuntaposx

Download the latest version of tuntaposx from 
[the official website](http://tuntaposx.sourceforge.net/download.xhtml)
and install the pkg.

You can check if the installation succeeded by looking for the existence of 
the `/dev/tun0` device and by running `kextstat`.

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

Warning: the setup of the `tun0` interface through ifconfig must be repeated
every time and it must be done after `./client.native` has been started;
because while the `/dev/tun0` device always exists, it is registered as a
network interface only after it is opened by a client.


## Tips

To monitor all messages being published over MQTT:

``mosquitto_sub -t '#' -v``

