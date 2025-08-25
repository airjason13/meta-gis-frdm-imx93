FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

PACKAGECONFIG:append = " vnc"


SRC_URI += "file://venom.jpg"

do_install:append() {

   install ${WORKDIR}/venom.jpg ${D}${datadir}/weston
}

FILES:${PN} += "${datadir/weston}"


FILES:${PN}:append = " ${sysconfdir}/pam.d/weston-remote-access"
