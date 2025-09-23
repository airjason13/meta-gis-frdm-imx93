
SUMMARY = "Python library for using asyncio in Qt-based applications"
HOMEPAGE = "None"
AUTHOR = "Arve Knudsen, Gerard Marull-Paretas, Mark Harviston, Alex March, Sam McCormack <Arve Knudsen <arve.knudsen@gmail.com>, Gerard Marull-Paretas <gerard@teslabs.com>, Mark Harviston <mark.harviston@gmail.com>, Alex March <alexmarch@fastmail.com>>"
LICENSE = "GPL2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=69e205dd99649bd02cfdd312d8161144"

SRC_URI = "https://files.pythonhosted.org/packages/ec/b2/5be08597dbbf331edb69478eae2f8dd511834cebf56a183b442e7437f8e0/qasync-0.28.0.tar.gz"
SRC_URI[md5sum] = "456a269c14576094ad8e2e7f1bb1948f"
SRC_URI[sha256sum] = "6f7f1f18971f59cb259b107218269ba56e3ad475ec456e54714b426a6e30b71d"

S = "${WORKDIR}/qasync-0.28.0"

RDEPENDS_${PN} = ""

# inherit setuptools3
inherit python_setuptools_build_meta pypi
