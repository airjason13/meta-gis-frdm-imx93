
SUMMARY = "MessagePack serializer"
HOMEPAGE = "None"
AUTHOR = "None <Inada Naoki <songofacandy@gmail.com>>"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://COPYING;md5=cd9523181d9d4fbf7ffca52eaa2a5751"

SRC_URI = "https://files.pythonhosted.org/packages/45/b1/ea4f68038a18c77c9467400d166d74c4ffa536f34761f7983a104357e614/msgpack-1.1.1.tar.gz"
SRC_URI[md5sum] = "abcd18fded80a89c486c0446f112eb06"
SRC_URI[sha256sum] = "77b79ce34a2bdab2594f490c8e80dd62a02d650b91a75159a63ec413b8d104cd"

S = "${WORKDIR}/msgpack-1.1.1"

RDEPENDS_${PN} = ""

inherit setuptools3
