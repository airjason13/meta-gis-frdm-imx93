DESCRIPTION = "Noto Sans Traditional Chinese Variable Font"
LICENSE = "CLOSED"
# LIC_FILES_CHKSUM = "file://LICENSE;md5=229eb75c6f04c2bdfa66aed9650627a0"

SRC_URI = "file://NotoSansTC-VariableFont_wght.ttf \
          "

S = "${WORKDIR}"

do_install() {
    install -d ${D}/usr/share/fonts/ttf
    install -m 0644 ${WORKDIR}/NotoSansTC-VariableFont_wght.ttf ${D}/usr/share/fonts/ttf/
}

FILES:${PN} += "/usr/share/fonts/ttf/"
