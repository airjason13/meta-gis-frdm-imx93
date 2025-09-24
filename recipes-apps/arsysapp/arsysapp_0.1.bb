LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://pyARSysApp \
            file://arsysapp.service \
            file://run_pyARSysApp.sh \
            "

S = "${WORKDIR}"

inherit systemd

do_install() {
        install -d ${D}${bindir}
	install -d ${D}/root/pyARSysApp/
        install -d ${D}${systemd_system_unitdir}
        cp -r pyARSysApp/* ${D}/root/pyARSysApp
        install -m 0644 ${S}/arsysapp.service ${D}${systemd_system_unitdir}/arsysapp.service
        install -m 0755 ${S}/run_pyARSysApp.sh ${D}${bindir}/run_pyARSysApp.sh
}

FILES:${PN} += " \
                /root/* \
                ${systemd_system_unitdir} \
                "

RDEPENDS:${PN} += "python3-core"
SYSTEMD_SERVICE:${PN} = "arsysapp.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"
