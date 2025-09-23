
SUMMARY = "lightweight wrapper around basic LLVM functionality"
HOMEPAGE = "http://llvmlite.readthedocs.io"
AUTHOR = "None <None>"
LICENSE = "BSD"
LIC_FILES_CHKSUM = "file://LICENSE;md5=a15ea9843f27327e08f3c5fbf8043a2b"

SRC_URI = "https://files.pythonhosted.org/packages/9d/73/4b29b502618766276816f2f2a7cf9017bd3889bc38a49319bee9ad492b75/llvmlite-0.45.0.tar.gz"
SRC_URI[md5sum] = "3b8fafb1cceec7791ba9ffe8a7167307"
SRC_URI[sha256sum] = "ceb0bcd20da949178bd7ab78af8de73e9f3c483ac46b5bef39f06a4862aa8336"

S = "${WORKDIR}/llvmlite-0.45.0"

RDEPENDS_${PN} = ""

inherit setuptools3
