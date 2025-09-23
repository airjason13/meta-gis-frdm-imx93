SUMMARY = "bitbake-layers recipe"
DESCRIPTION = "Recipe created by bitbake-layers"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${WORKDIR}/LICENSE;md5=e4ac654ba9b61686c2dc854a1128a323"
FILESEXTRAPATHS:append := "${THISDIR}/${PN}:"
SRC_URI += "file://qasync/ \
            file://qasync-0.28.0.dist-info \
            file://LICENSE \
             "
inherit python3-dir

S = "${WORKDIR}"

do_compile() {
}

do_install() {
    #qasync
    install -d ${D}${libdir}/${PYTHON_DIR}/site-packages/qasync
    install -d ${D}${libdir}/${PYTHON_DIR}/site-packages/qasync-0.28.0.dist-info
    cp -r ${S}/qasync/* ${D}${libdir}/${PYTHON_DIR}/site-packages/qasync/
    cp -r ${S}/qasync-0.28.0.dist-info/* ${D}${libdir}/${PYTHON_DIR}/site-packages/qasync-0.28.0.dist-info/

}

FILES:${PN} += " \
                ${libdir}/${PYTHON_DIR}/site-packages/* \
		${bindir} \
                "

