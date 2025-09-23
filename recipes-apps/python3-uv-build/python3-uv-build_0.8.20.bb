
SUMMARY = "The uv build backend"
HOMEPAGE = "https://pypi.org/project/uv/"
AUTHOR = "None <"Astral Software Inc." <hey@astral.sh>>"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE-MIT;md5=45674e482567aa99fe883d3270b11184"

SRC_URI = "https://files.pythonhosted.org/packages/88/8b/72efb552a56ab4bf3f7c3a81bf6abebe6b88a4b014b8dc9789807b1b740e/uv_build-0.8.20.tar.gz"
SRC_URI[md5sum] = "5379fa2ffc63bff102d9de1ba6160d28"
SRC_URI[sha256sum] = "49fde182052398c7780f4bb31d56a3ac38071a3f02646b38c92565e91da71a3d"

S = "${WORKDIR}/uv_build-0.8.20"

RDEPENDS_${PN} = ""

inherit setuptools3
