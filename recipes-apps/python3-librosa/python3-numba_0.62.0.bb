
SUMMARY = "compiling Python code using LLVM"
HOMEPAGE = "https://numba.pydata.org"
AUTHOR = "None <None>"
LICENSE = "BSD"
LIC_FILES_CHKSUM = "file://LICENSE;md5=d9bed34136e8491d5d792c7efc17f10c"

SRC_URI = "https://files.pythonhosted.org/packages/e5/96/66dae7911cb331e99bf9afe35703317d8da0fad81ff49fed77f4855e4b60/numba-0.62.0.tar.gz"
SRC_URI[md5sum] = "dcc842fef7248e226928efa834e19936"
SRC_URI[sha256sum] = "2afcc7899dc93fefecbb274a19c592170bc2dbfae02b00f83e305332a9857a5a"

S = "${WORKDIR}/numba-0.62.0"

RDEPENDS_${PN} = "python3-llvmlite python3-numpy"

inherit setuptools3
