LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://pyLightEngineApp \
            "


S = "${WORKDIR}"


do_install() {
	install -d ${D}/root/pyLightEngineApp/
        cp -r pyLightEngineApp/* ${D}/root/pyLightEngineApp
}

FILES:${PN} += " \
                /root/* \
                "

RDEPENDS:${PN} += "python3-core"
