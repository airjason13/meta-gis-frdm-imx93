
SUMMARY = "threadpoolctl"
HOMEPAGE = "https://github.com/joblib/threadpoolctl"
AUTHOR = "Thomas Moreau <thomas.moreau.2010@gmail.com>"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=8f2439cfddfbeebdb5cac3ae4ae80eaf"

SRC_URI = "https://files.pythonhosted.org/packages/b7/4d/08c89e34946fce2aec4fbb45c9016efd5f4d7f24af8e5d93296e935631d8/threadpoolctl-3.6.0.tar.gz"
SRC_URI[md5sum] = "201b8ab8d03c9154d24290fa8dccee85"
SRC_URI[sha256sum] = "8ab8b4aa3491d812b623328249fab5302a68d2d71745c8a4c719a2fcaba9f44e"

S = "${WORKDIR}/threadpoolctl-3.6.0"

RDEPENDS_${PN} = ""

inherit setuptools3
