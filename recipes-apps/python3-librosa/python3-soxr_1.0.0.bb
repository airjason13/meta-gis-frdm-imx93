
SUMMARY = "High quality, one-dimensional sample-rate conversion library"
HOMEPAGE = "None"
AUTHOR = "KEUM Myungchul <None>"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://COPYING.LGPL;md5=8c2e1ec1540fb3e0beb68361344cba7e"

SRC_URI = "https://files.pythonhosted.org/packages/42/7e/f4b461944662ad75036df65277d6130f9411002bfb79e9df7dff40a31db9/soxr-1.0.0.tar.gz"
SRC_URI[md5sum] = "090491d89b85584505e7fad623b51e01"
SRC_URI[sha256sum] = "e07ee6c1d659bc6957034f4800c60cb8b98de798823e34d2a2bba1caa85a4509"

S = "${WORKDIR}/soxr-1.0.0"

RDEPENDS_${PN} = "python3-numpy"

inherit setuptools3
