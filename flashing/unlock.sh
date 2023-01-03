#!/bin/bash
raspi-gpio set 27 op dh
sleep 1
raspi-gpio set 27 op dl
openocd -c "source [find interface/raspberrypi-native.cfg]" -f "flash_unprotect.ocd"
raspi-gpio set 27 op dh
