
SUMMARY = "A friend to fetch your data files"
HOMEPAGE = "None"
AUTHOR = "None <The Pooch Developers <fatiandoaterra@protonmail.com>>"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE.txt;md5=34edf66cdaea1acc8b8a430af829c7dc"

SRC_URI = "https://files.pythonhosted.org/packages/c6/77/b3d3e00c696c16cf99af81ef7b1f5fe73bd2a307abca41bd7605429fe6e5/pooch-1.8.2.tar.gz"
SRC_URI[md5sum] = "7a333ef27c34984385c25f1e0b156185"
SRC_URI[sha256sum] = "76561f0de68a01da4df6af38e9955c4c9d1a5c90da73f7e40276a5728ec83d10"

S = "${WORKDIR}/pooch-1.8.2"

RDEPENDS_${PN} = "python3-platformdirs python3-packaging python3-requests"

inherit setuptools3
