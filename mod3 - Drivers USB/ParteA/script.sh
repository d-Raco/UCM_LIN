#!/bin/bash

while true
do
  for ((i=0; $i<=6; i++))
  do
      echo $i:0x001100,$(($i+1)):0x000011 > /dev/usb/blinkstick0
      sleep 0.4
  done
done
