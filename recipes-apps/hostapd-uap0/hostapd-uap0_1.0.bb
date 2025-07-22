LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://hostapd_uap0.service \
            file://hostapd_uap0.sh \
            file://uap0_hostapd.conf \
            file://uap0_udhcpd.conf \
            "

S = "${WORKDIR}"

do_install() {
    install -d ${D}/${sysconfdir}/
    install -d ${D}/${libdir}/systemd/system/
    install -d ${D}/${bindir}/

    install -m 755 hostapd_uap0.sh ${D}/${bindir}/
    install -m 755 hostapd_uap0.service ${D}/${libdir}/systemd/system/
    install -m 755 uap0_hostapd.conf ${D}/${sysconfdir}/
    install -m 755 uap0_udhcpd.conf ${D}/${sysconfdir}/
    
}

FILES:${PN} += " \
                ${libdir}/* \
                "

