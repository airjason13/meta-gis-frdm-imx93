FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += "file://hwrevision \
	    file://swupdate.service \
           "


do_install:append() {
    install -m 0644 ${WORKDIR}/swupdate.service ${D}${systemd_system_unitdir}/
    install -m 0644 ${WORKDIR}/hwrevision ${D}${sysconfdir}/
}

