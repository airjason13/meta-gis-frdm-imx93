
SUMMARY = "Decorators for Humans"
HOMEPAGE = "None"
AUTHOR = "None <Michele Simionato <michele.simionato@gmail.com>>"
LICENSE = "BSD-2-Clause"
LIC_FILES_CHKSUM = "file://LICENSE.txt;md5=69f84fd117b2398674e12b8380df27c8"

SRC_URI = "https://files.pythonhosted.org/packages/43/fa/6d96a0978d19e17b68d634497769987b16c8f4cd0a7a05048bec693caa6b/decorator-5.2.1.tar.gz"
SRC_URI[md5sum] = "984649ae1fd174f9a82369e7c9cc56e6"
SRC_URI[sha256sum] = "65f266143752f734b0a7cc83c46f4618af75b8c5911b00ccb61d0ac9b6da0360"

S = "${WORKDIR}/decorator-5.2.1"

RDEPENDS_${PN} = ""

inherit setuptools3
