LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://pyFlaskMediaFileManager \
            file://flaskmediafilemanager.service \
            file://run_pyFlaskMediaFileManager.sh \
            "


S = "${WORKDIR}"

inherit systemd

do_install() {
        install -d ${D}${bindir}
	install -d ${D}/root/pyFlaskMediaFileManager/
        install -d ${D}${systemd_system_unitdir}
        cp -r pyFlaskMediaFileManager/* ${D}/root/pyFlaskMediaFileManager
        install -m 0644 ${S}/flaskmediafilemanager.service ${D}${systemd_system_unitdir}/flaskmediafilemanager.service
        install -m 0755 ${S}/run_pyFlaskMediaFileManager.sh ${D}${bindir}/run_pyFlaskMediaFileManager.sh
}

FILES:${PN} += " \
                /root/* \
                ${systemd_system_unitdir} \
                "

RDEPENDS:${PN} += "python3-core"

SYSTEMD_SERVICE:${PN} = "flaskmediafilemanager.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

