
SUMMARY = "Multi-library, cross-platform audio decoding."
HOMEPAGE = "None"
AUTHOR = "None <Adrian Sampson <adrian@radbox.org>>"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=61371466aff0e8af36ec79037481680b"

SRC_URI = "https://files.pythonhosted.org/packages/db/d2/87016ca9f083acadffb2d8da59bfa3253e4da7eeb9f71fb8e7708dc97ecd/audioread-3.0.1.tar.gz"
SRC_URI[md5sum] = "3de844f9c75b97691da85e0f1ec76e90"
SRC_URI[sha256sum] = "ac5460a5498c48bdf2e8e767402583a4dcd13f4414d286f42ce4379e8b35066d"

S = "${WORKDIR}/audioread-3.0.1"

RDEPENDS_${PN} = ""

inherit setuptools3
