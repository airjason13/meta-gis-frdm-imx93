LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://pyMessageServer \
            "


S = "${WORKDIR}"


do_install() {
	install -d ${D}/root/pyMessageServer/
        cp -r pyMessageServer/* ${D}/root/pyMessageServer
}

FILES:${PN} += " \
                /root/* \
                "

RDEPENDS:${PN} += "python3-core"
