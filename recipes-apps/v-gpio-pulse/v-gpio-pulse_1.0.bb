LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://app/ \
            file://utildbglib/ \
            file://Makefile \
            "

DEPENDS += " libgpiod "

S = "${WORKDIR}"

do_compile() {
    make
}

do_install() {
    install -d ${D}/${bindir}/

    install -m 777 prog/v-gpio-pulse ${D}/${bindir}/
    
}

TARGET_CC_ARCH += "${LDFLAGS}"
