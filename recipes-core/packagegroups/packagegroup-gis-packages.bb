SUMMARY = "All fonts packages "
PR = "r6"


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
    python3-pycairo \
    python3-pyaudio \
    python3-smbus2 \
    arglassesdemo \
    armessageserver \
    lightengineapp \
    arsysapp \
    flaskmediafilemanager \
    gisdemos \
    wifi-connect-mlan0 \
    alsa-dev \
    alsa-utils \
    alsa-tools \
    alsa-plugins \
    imx-alsa-plugins \
    alsa-state \
    libeigen-dev \
    wget \
    lsof \
    wireshark \
    ttf-dejavu-sans \
    ttf-dejavu-sans-mono \
    ttf-dejavu-sans-condensed \
    ttf-dejavu-serif \
    ttf-dejavu-serif-condensed \
    ttf-dejavu-mathtexgyre \
    fontconfig-utils \
    bluez5 \
    bluez-tools \
    python3-dbus \
    python3-pygobject \
    python3-netclient \
    python3-netserver \
    python3-io \
    python3-datetime \
    python3-logging \
    python3-asyncio \
    pipewire \
    wireplumber \
    pipewire-media-session \
    alsa-utils \
    jbd4040-reinit \
    jbd4040-reg \
"

PACKAGE_ARCH = "${TUNE_PKGARCH}"
