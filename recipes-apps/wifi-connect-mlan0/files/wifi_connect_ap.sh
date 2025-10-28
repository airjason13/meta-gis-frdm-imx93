#!/bin/sh
MLAN0_IFACE='mlan0'
echo $MLAN0_IFACE
ETH0_IFACE='eth0'
ETH1_IFACE='eth1'

CONF_FILE="/etc/wpa_supplicant_mlan0.conf"
DEFAULT_CONF_FILE="/etc/wpa_supplicant_default.conf"
FOUND_TARGET_SSID=0
RETRIES=10             # 最多重試次數
SLEEP_AFTER_SCAN=5    # 掃描後等待秒數

ip link set mlan0 up 
sleep 1
wpa_supplicant -B -Dnl80211 -i$MLAN0_IFACE -c$DEFAULT_CONF_FILE

# 檢查檔案是否存在
if [ ! -f "$CONF_FILE" ]; then
        echo "錯誤：找不到檔案 $CONF_FILE"
        exit 1
fi

echo "--- 檔案 $CONF_FILE 中的 SSID (使用 sed) ---"

# 使用 sed：
# 1. '/^\s*ssid=/!d' 刪除不包含 'ssid=' 的行。
# 2. 's/^\s*ssid="\(.*\)"/\1/p' 提取雙引號內的內容，並打印出來。
# 3. '-n' 只打印被 'p' 命令處理過的行。
ssid_value=$(sed -n '/^\s*ssid=/s/^\s*ssid="\(.*\)"/\1/p' "$CONF_FILE")

if [ -n "$ssid_value" ]; then
        echo "$ssid_value"
else
        echo "未在 network 區塊中找到 ssid。"
        exit 1
fi

ip link show mlan0

# 檢查 wpa_cli 是否可用，以及 wpa_supplicant 是否可連
if ! command -v wpa_cli >/dev/null 2>&1; then
        echo "[ERROR] wpa_cli not found. Install wpa_supplicant package." >&2
        exit 2
fi

if ! wpa_cli -i "$MLAN0_IFACE" status >/dev/null 2>&1; then
        echo "[WARN] wpa_cli cannot contact wpa_supplicant on interface $IFACE."
        echo "       Ensure wpa_supplicant is running and ctrl_interface path is accessible."
        # 仍嘗試掃描（有些系統可能回傳 FAIL）
fi

echo "could work"

KEYWORD=$ssid_value
echo "try to find "$KEYWORD
attempt=0
while (( attempt < RETRIES )); do
	((attempt++))
	echo "[*] Scan attempt ${attempt}/${RETRIES}..."

        # 啟動掃描
	if ! wpa_cli -i "$MLAN0_IFACE" scan >/dev/null 2>&1; then
                echo "[WARN] wpa_cli scan failed on attempt ${attempt}."
                sleep 1
                continue
        fi

        # 等待 driver 做掃描（可視情況增加秒數）
        sleep "$SLEEP_AFTER_SCAN"

    	# 取得掃描結果（去掉標頭行），並把前四欄去掉，只留下 SSID（SSID 可能含空格）
	scan_results=$(wpa_cli -i "$MLAN0_IFACE" scan_results 2>/dev/null || true)
        if [[ -z "$scan_results" ]]; then
                echo "[WARN] No scan_results returned on attempt ${attempt}."
                sleep 1
                continue
        fi
	# 解析 SSID（移除前4欄：BSSID FREQ SIGNAL FLAGS）
        # 並檢查是否有包含關鍵字（不分大小寫）
        # 使用 awk 來保留 SSID 內可能的空格
        matched=false
        echo "$scan_results" | sed '1d' | while IFS= read -r line; do
                # remove first 4 whitespace-separated fields
                ssid="$(echo "$line" | awk '{ $1=""; $2=""; $3=""; $4=""; sub(/^ +/, ""); print }')"
                if [[ -n "$ssid" ]] && echo "$ssid" | grep -i -q "$KEYWORD"; then
                        echo "[*] Match: SSID='${ssid}'"
                        matched=true
                        # we cannot call next_step here because this while runs in a subshell
                        # instead we print a marker and break
			# echo "__IPCAM_FOUND__"
                        echo "found "$KEYWORD
                        break
                fi
        done > /tmp/scan_parse.$$ 2>/dev/null
	# check for marker
        if grep -q "$KEYWORD" /tmp/scan_parse.$$ 2>/dev/null; then
                rm -f /tmp/scan_parse.$$
		echo found $KEYWORD
                FOUND_TARGET_SSID=1
                break
        else
                # optionally print parsed SSIDs for debugging
                echo "[*] No matching SSID on attempt ${attempt}."
                # show parsed SSIDs (skip marker)
                sed 'found $KEYWORD/d' /tmp/scan_parse.$$ || true
                rm -f /tmp/scan_parse.$$
		# exit 2
        fi

        # 等候一點再重試
        sleep 1
done

if [ "$FOUND_TARGET_SSID" -eq 1 ]; then
	echo "Found Target SSID"
else
	echo "No Target SSID"
	exit 2
fi

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

	RETRY=0
	MAX_RETRY=30
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
	echo "nameserver 8.8.8.8" > /etc/resolv.conf
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
