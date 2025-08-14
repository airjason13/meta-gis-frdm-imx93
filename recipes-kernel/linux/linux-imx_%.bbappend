FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://imx93-gis-frdm-dsi.dts \
            file://waveshare-5inch-dsi-lcd-b.cfg \
	    file://0001-Add-raspberrypi-7inch-panel-desc-and-mode.patch \           
	    file://0002-Add-panel-and-reg_bridge-in-imx93.dtsi.patch \
	    file://imx93-gis-frdm.dts \
	    "


KERNEL_DEVICETREE:append = " freescale/imx93-gis-frdm-dsi.dtb \
                            "

KERNEL_CONFIG_FRAGMENTS += "waveshare-5inch-dsi-lcd-b.cfg"

do_patch:append() {
    install -Dm0644 ${WORKDIR}/imx93-gis-frdm-dsi.dts \
        ${S}/arch/arm64/boot/dts/freescale/imx93-gis-frdm-dsi.dts
    install -Dm0644 ${WORKDIR}/imx93-gis-frdm.dts \
        ${S}/arch/arm64/boot/dts/freescale/imx93-gis-frdm.dts
}
    
do_configure:append() {
    cat ../*.cfg >> ${B}/.config
}
