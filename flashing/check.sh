#!/bin/bash
raspi-gpio set 27 op dh
sleep 1
raspi-gpio set 27 op dl
openocd -c "source [find interface/raspberrypi-native.cfg]" -f "check_approtect.ocd"
raspi-gpio set 27 op dh
