SUMMARY = "All fonts packages "
PR = "r5"


inherit packagegroup features_check



RDEPENDS:${PN} = " \
    lua \
    swupdate \
    swupdate-www \
    swupdate-progress \
    swupdate-client \
    swupdate-tools-ipc \
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
    tree \
    libubootenvgis \
    ffmpeg \
    git \
    ninja \
    android-tools \
    python3-flask \
    python3-ffmpy \
    python3-qasync \
    arglassesdemo \
    armessageserver \
    lightengineapp \
    arsysapp \
    flaskmediafilemanager \
    gisdemos \
    ttf-dejavu-sans \
    ttf-dejavu-sans-mono \
    ttf-dejavu-sans-condensed \
    ttf-dejavu-serif \
    ttf-dejavu-serif-condensed \
    ttf-dejavu-mathtexgyre \
    fontconfig-utils \
"
