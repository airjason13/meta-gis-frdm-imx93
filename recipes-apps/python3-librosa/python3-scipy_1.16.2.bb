
SUMMARY = "Fundamental algorithms for scientific computing in Python"
HOMEPAGE = "None"
AUTHOR = "None <None>"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE.txt;md5=6506a2e1578b1a1161d9bda0b896c647"

SRC_URI = "https://files.pythonhosted.org/packages/4c/3b/546a6f0bfe791bbb7f8d591613454d15097e53f906308ec6f7c1ce588e8e/scipy-1.16.2.tar.gz"
SRC_URI[md5sum] = "ccfa8c999114388be9caae83a01296e5"
SRC_URI[sha256sum] = "af029b153d243a80afb6eabe40b0a07f8e35c9adc269c019f364ad747f826a6b"

S = "${WORKDIR}/scipy-1.16.2"

RDEPENDS_${PN} = "python3-numpy"

inherit setuptools3
