#!/bin/bash
die () {
    echo >&2 "$@"
    exit 1
}

[ "$#" -eq 1 ] || die "1 argument required, $# provided"

raspi-gpio set 27 op dh
sleep 1
raspi-gpio set 27 op dl
openocd -c "source [find interface/raspberrypi-native.cfg]" -c "transport select swd" -c "source [find target/nrf52.cfg]" -c "init" -c "reset" -c "halt" -c "program $1 verify exit"
raspi-gpio set 27 op dh
