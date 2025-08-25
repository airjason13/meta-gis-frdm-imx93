LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://fw_env.config \
            file://u-boot-initial-env \
            "

# S = "${WORKDIR}"

do_install() {
    install -d ${D}/${sysconfdir}/

    install -m 666 ${WORKDIR}/fw_env.config ${D}/${sysconfdir}/	
    install -m 666 ${WORKDIR}/u-boot-initial-env ${D}/${sysconfdir}/	
}		
