
SUMMARY = "An audio library based on libsndfile, CFFI and NumPy"
HOMEPAGE = "https://github.com/bastibe/python-soundfile"
AUTHOR = "Bastian Bechtold <basti@bastibe.de>"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=50c87fd45f9555bc1ffe01fe22be78d2"

SRC_URI = "https://files.pythonhosted.org/packages/e1/41/9b873a8c055582859b239be17902a85339bec6a30ad162f98c9b0288a2cc/soundfile-0.13.1.tar.gz"
SRC_URI[md5sum] = "06cf02ab567ac3fbfa4510ada79bb46f"
SRC_URI[sha256sum] = "b2c68dab1e30297317080a5b43df57e302584c49e2942defdde0acccc53f0e5b"

S = "${WORKDIR}/soundfile-0.13.1"

RDEPENDS_${PN} = "python3-cffi python3-numpy"

inherit setuptools3
