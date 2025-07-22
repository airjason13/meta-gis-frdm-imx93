LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://v-gpio-detect.c \
            file://Makefile \
            "

DEPENDS += " libgpiod "

S = "${WORKDIR}"

do_compile() {
    make
}

do_install() {
    install -d ${D}/${bindir}/

    install -m 777 v-gpio-detect ${D}/${bindir}/
    
}

TARGET_CC_ARCH += "${LDFLAGS}"
