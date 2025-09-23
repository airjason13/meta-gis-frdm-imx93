LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://pyARGlassesDemo \
            "


S = "${WORKDIR}"


do_install() {
	install -d ${D}/root/pyARGlassesDemo/
        cp -r pyARGlassesDemo/* ${D}/root/pyARGlassesDemo
}

FILES:${PN} += " \
                /root/* \
                "

RDEPENDS:${PN} += "python3-core"
