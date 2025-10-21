LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://wifi_connect_mlan0.service \
            file://wifi_connect_ap.sh \
            file://wpa_supplicant_mlan0.conf \
            "

inherit systemd
SYSTEMD_SERVICE:${PN} = "wifi_connect_mlan0.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

S = "${WORKDIR}"

do_install() {
    install -d ${D}/${sysconfdir}/
    install -d ${D}/${systemd_system_unitdir}/
    install -d ${D}/${bindir}/

    install -m 777 wifi_connect_ap.sh ${D}/${bindir}/
    install -m 644 wifi_connect_mlan0.service ${D}/${systemd_system_unitdir}/
    install -m 644 wpa_supplicant_mlan0.conf ${D}/${sysconfdir}/
    
}


FILES:${PN} += " \
                ${systemd_system_unitdir} \
                "

