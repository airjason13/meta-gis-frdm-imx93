FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://0001-Add-waveshare-800x480-panel-support.patch \
            file://panel-ws-800x480.cfg \
            file://panel-ws-800x480.c \
            file://imx93-gis-frdm-dsi.dts \
            "


KERNEL_DEVICETREE:append = " freescale/imx93-gis-frdm-dsi.dtb \
                            "

KERNEL_CONFIG_FRAGMENTS += "panel-ws-800x480.cfg"

do_patch:append() {
    install -Dm0644 ${WORKDIR}/panel-ws-800x480.c \
        ${S}/drivers/gpu/drm/panel/panel-ws-800x480.c
    install -Dm0644 ${WORKDIR}/imx93-gis-frdm-dsi.dts \
        ${S}/arch/arm64/boot/dts/freescale/imx93-gis-frdm-dsi.dts
}
    
do_configure:append() {
    cat ../*.cfg >> ${B}/.config
}
