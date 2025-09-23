
SUMMARY = "Internationalized Domain Names in Applications (IDNA)"
HOMEPAGE = "None"
AUTHOR = "None <Kim Davies <kim+pypi@gumleaf.org>>"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE.md;md5=204c0612e40a4dd46012a78d02c80fb1"

SRC_URI = "https://files.pythonhosted.org/packages/f1/70/7703c29685631f5a7590aa73f1f1d3fa9a380e654b86af429e0934a32f7d/idna-3.10.tar.gz"
SRC_URI[md5sum] = "28448b00665099117b6daa9887812cc4"
SRC_URI[sha256sum] = "12f65c9b470abda6dc35cf8e63cc574b1c52b11df2c86030af0ac09b01b13ea9"

S = "${WORKDIR}/idna-3.10"

RDEPENDS_${PN} = ""

inherit setuptools3
