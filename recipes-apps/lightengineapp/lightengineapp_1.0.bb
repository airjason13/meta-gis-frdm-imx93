LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://pyLightEngineApp \
            file://lightengine.service \
            file://run_pyLightEngineApp.sh \
            "

S = "${WORKDIR}"

inherit systemd

do_install() {
        install -d ${D}${bindir}
	install -d ${D}/root/pyLightEngineApp/
        install -d ${D}${systemd_system_unitdir}
        cp -r pyLightEngineApp/* ${D}/root/pyLightEngineApp
        install -m 0644 ${S}/lightengine.service ${D}${systemd_system_unitdir}/lightengine.service
        install -m 0755 ${S}/run_pyLightEngineApp.sh ${D}${bindir}/run_pyLightEngineApp.sh
}

FILES:${PN} += " \
                /root/* \
                ${systemd_system_unitdir} \
                "

RDEPENDS:${PN} += "python3-core"
SYSTEMD_SERVICE:${PN} = "lightengine.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"
