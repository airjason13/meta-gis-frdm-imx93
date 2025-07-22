#!/bin/sh

modprobe moal mod_para=nxp/wifi_mod_para.conf
sleep 2
ifconfig uap0 up

ifconfig uap0 192.168.1.2


killall udhcpd
sleep 1
udhcpd /etc/uap0_udhcpd.conf
sleep 1

killall hostapd
sleep 1
hostapd /etc/uap0_hostapd.conf

