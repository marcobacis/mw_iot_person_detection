#!/usr/bin/env python3

'''
This tool connects to a MQTT broker, subscribes to messages from iot/position,
and computes if the device is moving or not from the acceleration values
contained in the MQTT messages received.
It is intended to allow rapid iteration and testing of different values
of T and G.
'''

import sys
import json
import traceback
import numpy as np
from math import *

threshold = 1000
delta_threshold = 500
period = 2

old_modulo = 0
old_alfa = 0
old_gamma = 0
old_moving = 0

current_duration = 0

if len(sys.argv) < 2:
  print("usage:", sys.argv[0], "<mqtt broker address>");
  exit(0);

with open(sys.argv[1]) as f:
    content = f.readlines()

# you may also want to remove whitespace characters like `\n` at the end of each line
content = [x.strip() for x in content] 

durations = []

for msg in content:

  try:
    data = json.loads(msg)
    accel = json.loads('[' + data['last_accel'] + ']')
  
    gravity = 99
    modulo = abs(accel[0]**2 + accel[1]**2 + accel[2]**2 - gravity**2)
    alfa = atan2(accel[1], accel[0])
    gamma = atan2(-accel[0], sqrt(accel[1]**2 + accel[2]**2))
    delta_alfa = abs(alfa - old_alfa)
    delta_gamma = abs(gamma - old_gamma)
    delta_modulo = abs(modulo - old_modulo)
  
    #moving = (modulo >= 1000 or delta_modulo >= 500 or delta_alfa >= pi/12 or delta_gamma >= pi/12)
    moving = (modulo >= threshold or delta_modulo >= delta_threshold)
  
    """print('{:<20} {:>8} {:>8} {:>5.2f} {:>5.2f} {:>5.2f} {:>5.2f} is moving? {}'.format(
          str(accel), 
          modulo, delta_modulo, alfa, delta_alfa, gamma, delta_gamma, 
          moving))"""
  
    

    if moving and (not old_moving):
      current_duration = period
    elif (not moving) and old_moving:
      durations += [current_duration]
      current_duration = 0
    elif moving and old_moving:
      current_duration += period
    elif (not moving) and (not old_moving):
      current_duration = 0

    old_alfa = alfa
    old_gamma = gamma
    old_modulo = modulo
    old_moving = moving

  except:
    traceback.print_exc()

average = np.mean(durations)
stdev = np.std(durations)
shortest = np.min(durations)
longest = np.max(durations)

G = average - stdev

print("G ", G, ", average ", average, " std ", stdev, ", shortest ", shortest, ", longest ", longest)
