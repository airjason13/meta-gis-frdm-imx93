
SUMMARY = "A small Python package for determining appropriate platform-specific dirs, e.g. a `user data dir`."
HOMEPAGE = "None"
AUTHOR = "None <None>"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=ea4f5a41454746a9ed111e3d8723d17a"

SRC_URI = "https://files.pythonhosted.org/packages/23/e8/21db9c9987b0e728855bd57bff6984f67952bea55d6f75e055c46b5383e8/platformdirs-4.4.0.tar.gz"
SRC_URI[md5sum] = "57c995d9e18d9ef42f784b597b72deb4"
SRC_URI[sha256sum] = "ca753cf4d81dc309bc67b0ea38fd15dc97bc30ce419a7f58d13eb3bf14c4febf"

S = "${WORKDIR}/platformdirs-4.4.0"

RDEPENDS_${PN} = ""

inherit setuptools3
