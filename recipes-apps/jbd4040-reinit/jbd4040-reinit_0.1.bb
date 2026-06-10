SUMMARY = "JBD4040 Display I2C Driver Reset Sequence Service"
DESCRIPTION = "Systemd service and script to reinitialize jbd4040 at boot"
LICENSE = "CLOSED"

SRC_URI = " \
    file://jbd4040-reinit.service \
    file://jbd4040-reinit.sh \
"

# 告訴 BitBake 我們要使用 systemd 功能
inherit systemd

# 指定要交給 systemd 管理的 service 檔案名稱
SYSTEMD_SERVICE:${PN} = "jbd4040-reinit.service"
# 預設行為就是 enable，這行寫出來可以確保開機自動啟動
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

# 因為沒有原始碼需要編譯，指向 WORKDIR 即可
S = "${WORKDIR}"

do_install() {
    # 1. 安裝 Shell Script 到 /usr/bin/ (與你的 service 檔案內 ExecStart 路徑一致)
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/jbd4040-reinit.sh ${D}${bindir}/

    # 2. 安裝 Systemd Service 到 /lib/systemd/system/
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/jbd4040-reinit.service ${D}${systemd_system_unitdir}/
}

# 確保這些檔案有被正確打包進最終的 package 中
FILES:${PN} += "${bindir}/jbd4040-reinit.sh"
FILES:${PN} += "${systemd_system_unitdir}/jbd4040-reinit.service"
