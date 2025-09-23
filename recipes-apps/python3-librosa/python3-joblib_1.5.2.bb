
SUMMARY = "Lightweight pipelining with Python functions"
HOMEPAGE = "None"
AUTHOR = "None <Gael Varoquaux <gael.varoquaux@normalesup.org>>"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE.txt;md5=2e481820abf0a70a18011a30153df066"

SRC_URI = "https://files.pythonhosted.org/packages/e8/5d/447af5ea094b9e4c4054f82e223ada074c552335b9b4b2d14bd9b35a67c4/joblib-1.5.2.tar.gz"
SRC_URI[md5sum] = "560040af32080ce8c4b092a2cd320e26"
SRC_URI[sha256sum] = "3faa5c39054b2f03ca547da9b2f52fde67c06240c31853f306aea97f13647b55"

S = "${WORKDIR}/joblib-1.5.2"

RDEPENDS_${PN} = ""

inherit setuptools3
