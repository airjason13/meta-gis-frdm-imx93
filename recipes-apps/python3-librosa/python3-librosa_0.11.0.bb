
SUMMARY = "Python module for audio and music processing"
HOMEPAGE = "https://librosa.org"
AUTHOR = "Brian McFee, librosa development team <brian.mcfee@nyu.edu>"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://LICENSE.md;md5=25507c38058deac1239f865af7e6ed72"

SRC_URI = "https://files.pythonhosted.org/packages/64/36/360b5aafa0238e29758729e9486c6ed92a6f37fa403b7875e06c115cdf4a/librosa-0.11.0.tar.gz"
SRC_URI[md5sum] = "2d49565e5eaacbee0be753fbfd1aab37"
SRC_URI[sha256sum] = "f5ed951ca189b375bbe2e33b2abd7e040ceeee302b9bbaeeffdfddb8d0ace908"

S = "${WORKDIR}/librosa-0.11.0"

RDEPENDS_${PN} = "python3-audioread python3-numba python3-numpy python3-scipy python3-scikit-learn python3-joblib python3-decorator python3-soundfile python3-pooch python3-soxr python3-typing-extensions python3-lazy-loader python3-msgpack"

inherit setuptools3
