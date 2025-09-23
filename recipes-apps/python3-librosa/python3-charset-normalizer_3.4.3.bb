
SUMMARY = "The Real First Universal Charset Detector. Open, modern and actively maintained alternative to Chardet."
HOMEPAGE = "None"
AUTHOR = "None <"Ahmed R. TAHRI" <tahri.ahmed@proton.me>>"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=48178f3fc1374ad7e830412f812bde05"

SRC_URI = "https://files.pythonhosted.org/packages/83/2d/5fd176ceb9b2fc619e63405525573493ca23441330fcdaee6bef9460e924/charset_normalizer-3.4.3.tar.gz"
SRC_URI[md5sum] = "773b693324f251206cc5dcbec7dd2d4c"
SRC_URI[sha256sum] = "6fce4b8500244f6fcb71465d4a4930d132ba9ab8e71a7859e6a5d59851068d14"

S = "${WORKDIR}/charset_normalizer-3.4.3"

RDEPENDS_${PN} = ""

inherit setuptools3
