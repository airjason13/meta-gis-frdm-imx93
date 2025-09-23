LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://pyFlaskMediaFileManager \
            "


S = "${WORKDIR}"


do_install() {
	install -d ${D}/root/pyFlaskMediaFileManager/
        cp -r pyFlaskMediaFileManager/* ${D}/root/pyFlaskMediaFileManager
}

FILES:${PN} += " \
                /root/* \
                "

RDEPENDS:${PN} += "python3-core"
