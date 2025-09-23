
SUMMARY = "A simple Python wrapper for ffmpeg"
HOMEPAGE = "https://github.com/Ch00k/ffmpy"
AUTHOR = "Andrii Yurchuk <ay@mntw.re>"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://setup.py;md5=ee5dc06ef2e45fffc50b9dd40f56d18d"

SRC_URI = "https://files.pythonhosted.org/packages/bf/e2/947df4b3d666bfdd2b0c6355d215c45d2d40f929451cb29a8a2995b29788/ffmpy-0.3.0.tar.gz"
SRC_URI[md5sum] = "633492c6355d857a1c043f490e44e42f"
SRC_URI[sha256sum] = "757591581eee25b4a50ac9ffb9b58035a2794533db47e0512f53fb2d7b6f9adc"

S = "${WORKDIR}/ffmpy-0.3.0"

RDEPENDS_${PN} = ""

inherit setuptools3
