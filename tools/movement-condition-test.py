#!/usr/bin/env python3

'''
This tool connects to a MQTT broker, subscribes to messages from iot/position,
and computes if the device is moving or not from the acceleration values
contained in the MQTT messages received.
It is intended to allow rapid iteration and testing of different values
of T and G.
'''

import paho.mqtt.client as mqtt
import sys
import json
import traceback
from math import *


def on_connect(client, userdata, flags, rc):
  print("Connected with result code "+str(rc))
  client.subscribe("iot/position/#")


old_modulo = 0
old_alfa = 0
old_gamma = 0

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
  global old_alfa, old_gamma, old_modulo
  
  try:
    data = json.loads(msg.payload.decode('utf-8'))
    accel = json.loads('[' + data['last_accel'] + ']')
  
    gravity = 99
    modulo = abs(accel[0]**2 + accel[1]**2 + accel[2]**2 - gravity**2)
    alfa = atan2(accel[1], accel[0])
    gamma = atan2(-accel[0], sqrt(accel[1]**2 + accel[2]**2))
    delta_alfa = abs(alfa - old_alfa)
    delta_gamma = abs(gamma - old_gamma)
    delta_modulo = abs(modulo - old_modulo)
  
    #moving = (modulo >= 1000 or delta_modulo >= 500 or delta_alfa >= pi/12 or delta_gamma >= pi/12)
    moving = (modulo >= 1000 or delta_modulo >= 500)
  
    print('{:<20} {:>8} {:>8} {:>5.2f} {:>5.2f} {:>5.2f} {:>5.2f} is moving? {}'.format(
          str(accel), 
          modulo, delta_modulo, alfa, delta_alfa, gamma, delta_gamma, 
          moving))
  
    old_alfa = alfa
    old_gamma = gamma
    old_modulo = modulo
  except:
    traceback.print_exc()


if len(sys.argv) < 2:
  print("usage:", sys.argv[0], "<mqtt broker address>");
  exit(0);

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(sys.argv[1], 1883, 60)

client.loop_forever()
