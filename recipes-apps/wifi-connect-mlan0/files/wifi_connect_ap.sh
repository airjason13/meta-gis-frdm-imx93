#!/bin/sh
MLAN0_IFACE='mlan0'
echo $MLAN0_IFACE
ETH0_IFACE='eth0'
ETH1_IFACE='eth1'
while true; do
	STATUS=$(wpa_cli -i mlan0 status | grep wpa_state= | cut -d= -f2)
	echo $STATUS
	if [ "$STATUS" == "COMPLETED" ]; then	
		echo "mlan0 wifi connected"
		if ip link show "$MLAN0_IFACE" | grep "state UP"; then
 			echo "$MLAN0_IFACE is UP"
			break
		fi
	fi
	
	pkill -f $MLAN0_IFACE
	pkill -f $ETH0_IFACE
	pkill -f $ETH1_IFACE
	# ifconfig mlan0 down
	# ip link set mlan0 down
	sleep 3

	while true; do
		ifconfig $MLAN0_IFACE up
		ip link set mlan0 up
		ip link show mlan0
		if ip link show "$MLAN0_IFACE"; then
			echo "mlan0 up"
			break
		else
			ifconfig mlan0 down
			echo "mlan0 down"
		fi
		
		sleep 3
	done

	echo "wpa_up"

	# mlan0 up
	ifconfig $MLAN0_IFACE up
	ip link set $MLAN0_IFACE up
	# while true; do
	# 	if ip link show "$MLAN0_IFACE" | grep -q "state UP"; then
	# 		echo "$MLAN0_IFACE is UP"
	#		break
	#	else
	#		echo "$MLAN0_IFACE is DOWN"	
	#		ifconfig $MLAN0_IFACE up
	#		ip link show $MLAN0_IFACE
	#	fi
	#	sleep 3
	#done

	RETRY=0
	MAX_RETRY=30
	#wpa_supplicant -Dnl80211 -i$MLAN0_IFACE -c/etc/wpa_supplicant_mlan0.conf &
	setsid wpa_supplicant -B -Dnl80211 -i$MLAN0_IFACE -c/etc/wpa_supplicant_mlan0.conf & 
	while [ $RETRY -lt $MAX_RETRY ]; do
		sleep 10
		STATUS=$(wpa_cli -i mlan0 status | grep wpa_state= | cut -d= -f2)
		echo $STATUS
		if [ "$STATUS" == "COMPLETED" ]; then	
			echo "mlan0 wifi connected"
			break
		fi
		RETRY=$((RETRY+1))	
	done

	setsid udhcpc -i $MLAN0_IFACE  & 
	sleep 2
done
echo "check 1"
ip link show mlan0

if ip link show "$ETH0_IFACE" | grep "state DOWN"; then
	echo "Interface $ETH0_IFACE not found"
	ifconfig $ETH0_IFACE up
	setsid udhcpc -i $ETH0_IFACE >/dev/null 2>&1 &
fi

echo "check 2"
ip link show mlan0

if ip link show "$ETH1_IFACE" | grep "state DOWN"; then
	echo "Interface $ETH1_IFACE not found"
	ifconfig $ETH1_IFACE up
	setsid udhcpc -i $ETH1_IFACE >/dev/null 2>&1 &
fi

echo "nameserver 8.8.8.8" > /etc/resolv.conf
#while true; do
#	sleep 3
#done
