#!/bin/sh

modprobe moal mod_para=nxp/wifi_mod_para.conf
sleep 2
ifconfig uap0 up

ifconfig uap0 192.168.1.2


killall udhcpd
sleep 1
udhcpd /etc/uap0_udhcpd.conf
sleep 1

# 讀取編譯時儲存的時間戳記 ($MM$DD$hh$mm)
if [ -f /etc/build_timestamp ]; then
    TS=$(cat /etc/build_timestamp | tr -d '\r\n')
else
    TS=$(grep '^ssid=' /etc/uap0_hostapd.conf | grep -o '[0-9]\{8\}$')
fi

# 依據硬體節點動態調整 SSID
if [ -n "$TS" ]; then
    if [ -e /dev/jbd4040 ]; then
        SSID_NAME="GiS_AR_B${TS}"
    elif [ -e /dev/jbd4020 ]; then
        SSID_NAME="GiS_AR_A${TS}"
    else
        SSID_NAME="GiS_AR_${TS}"
    fi
    sed -i "s/^ssid=.*/ssid=${SSID_NAME}/" /etc/uap0_hostapd.conf
fi

killall hostapd
sleep 1
hostapd /etc/uap0_hostapd.conf

