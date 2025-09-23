LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://pyMessageServer \
            file://armessageserver.service \
            file://run_pyMessageServer.sh \ 
           "


S = "${WORKDIR}"

inherit systemd

do_install() {
        install -d ${D}${bindir}
        install -d ${D}${systemd_system_unitdir}
	install -d ${D}/root/pyMessageServer/
        cp -r pyMessageServer/* ${D}/root/pyMessageServer
        install -m 0644 ${S}/armessageserver.service ${D}${systemd_system_unitdir}/armessageserver.service
        install -m 0755 ${S}/run_pyMessageServer.sh ${D}${bindir}/run_pyMessageServer.sh
}

FILES:${PN} += " \
                /root/* \
                ${systemd_system_unitdir} \
                "

RDEPENDS:${PN} += "python3-core"

SYSTEMD_SERVICE:${PN} = "armessageserver.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"
