
SUMMARY = "A set of python modules for machine learning and data mining"
HOMEPAGE = "None"
AUTHOR = "None <None>"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://COPYING;md5=c0c49304a5cb997fd012292beee4ddce"

SRC_URI = "https://files.pythonhosted.org/packages/98/c2/a7855e41c9d285dfe86dc50b250978105dce513d6e459ea66a6aeb0e1e0c/scikit_learn-1.7.2.tar.gz"
SRC_URI[md5sum] = "c90ef381897c1aebe83ebf0c5829fe31"
SRC_URI[sha256sum] = "20e9e49ecd130598f1ca38a1d85090e1a600147b9c02fa6f15d69cb53d968fda"

S = "${WORKDIR}/scikit_learn-1.7.2"

RDEPENDS_${PN} = "python3-numpy python3-scipy python3-joblib python3-threadpoolctl"

inherit setuptools3
