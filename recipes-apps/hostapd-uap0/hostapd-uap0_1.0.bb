LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://hostapd_uap0.service \
            file://hostapd_uap0.sh \
            file://uap0_hostapd.conf \
            file://uap0_udhcpd.conf \
            "

inherit systemd
SYSTEMD_SERVICE:${PN} = "hostapd_uap0.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

S = "${WORKDIR}"

do_install() {
    install -d ${D}/${sysconfdir}/
    install -d ${D}/${systemd_system_unitdir}/
    install -d ${D}/${bindir}/

    install -m 777 hostapd_uap0.sh ${D}/${bindir}/
    install -m 644 hostapd_uap0.service ${D}/${systemd_system_unitdir}/
    install -m 644 uap0_hostapd.conf ${D}/${sysconfdir}/
    install -m 644 uap0_udhcpd.conf ${D}/${sysconfdir}/
    
}


FILES:${PN} += " \
                ${systemd_system_unitdir} \
                "

