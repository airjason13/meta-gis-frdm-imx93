LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://pyARGlassesDemo \
            file://arglassesdemo.service \
            file://run_pyARGlassesDemo.sh \ 
           "


S = "${WORKDIR}"

inherit systemd

do_install() {
        install -d ${D}${bindir}
        install -d ${D}${systemd_system_unitdir}
	install -d ${D}/root/pyARGlassesDemo/
        cp -r pyARGlassesDemo/* ${D}/root/pyARGlassesDemo
        install -m 0644 ${S}/arglassesdemo.service ${D}${systemd_system_unitdir}/arglassesdemo.service
        install -m 0755 ${S}/run_pyARGlassesDemo.sh ${D}${bindir}/run_pyARGlassesDemo.sh
}

FILES:${PN} += " \
                /root/* \
                ${systemd_system_unitdir} \
                "

RDEPENDS:${PN} += "python3-core"

SYSTEMD_SERVICE:${PN} = "arglassesdemo.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"
