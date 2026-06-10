#!/bin/sh

DRM_STATUS="/sys/class/drm/card0-DSI-1/status"

sleep 5

# 1. 輸出日誌到 dmesg 方便除錯
echo "jbd4040_script: Starting boot reset sequence..." > /dev/kmsg

# 2. 等待 DRM 節點出現 (最多等待 10 秒，避免死迴圈卡住)
WAIT_COUNT=0
while [ ! -f "$DRM_STATUS" ]; do
    sleep 1
    WAIT_COUNT=$((WAIT_COUNT + 1))
    if [ $WAIT_COUNT -ge 10 ]; then
        echo "jbd4040_script: Error - DRM node not found after 10s. Exiting." > /dev/kmsg
        exit 1
    fi
done

echo "jbd4040_script: DRM node found, proceeding with reset." > /dev/kmsg

# 3. 執行重啟流程 (加上指令的絕對路徑)
echo off > "$DRM_STATUS"
sleep 1

/sbin/rmmod i2c-jbd4040
sleep 1

/sbin/modprobe i2c-jbd4040
sleep 1

echo on > "$DRM_STATUS"

echo "jbd4040_script: Reset sequence completed successfully." > /dev/kmsg
