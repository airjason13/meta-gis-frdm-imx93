PACKAGECONFIG:append:pn-wireshark:class-target = " qt5 "
PACKAGECONFIG:remove:pn-wireshark:class-native = " qt5 "

inherit ${@bb.utils.contains('PACKAGECONFIG', 'qt5', 'cmake_qt5', '', d)}


EXTRA_OECMAKE:append:pn-wireshark:class-target = " -DUSE_qt6=off"

