SUMMARY = "All fonts packages "
PR = "r5"


inherit packagegroup features_check


# ffmpeg/ffplay
PREFERRED_VERSION:pn-ffmpeg = "6.1.1"
LICENSE_FLAGS_ACCEPTED="commercial"
PACKAGECONFIG:append:pn-ffmpeg = " v4l2 "
PACKAGECONFIG:append:pn-ffmpeg = " fdk-aac jack gpl x264 xcb xv sdl2"

RDEPENDS:${PN} = " \
    swupdate swupdate-www \
    dtc \
    u-boot-fw-utils \
    htop \
    atop \
    gstreamer1.0-python \
    libgpiod-dev \
    python3-gpiod \
    hostapd-uap0 \
    v-gpio-detect \
    v-gpio-pulse \
    python3-pyqt5 \
    qtimageformats \
    qtmultimedia \
    qtquickcontrols \
    qtquickcontrols2 \
    qtdeclarative \
    qtgraphicaleffects \
    qtbase \
    qtquick3d \
    qt5ledscreen \
    vim \
"
