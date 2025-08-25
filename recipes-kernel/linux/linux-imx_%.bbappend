FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://imx93-gis-frdm-dsi.dts \
            file://waveshare-5inch-dsi-lcd-b.cfg \
	    file://0001-Add-raspberrypi-7inch-panel-desc-and-mode.patch \           
	    file://0002-Add-panel-and-reg_bridge-in-imx93.dtsi.patch \
	    file://imx93-gis-frdm.dts \
	    file://gis.ppm \
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
    cp ../gis.ppm ${S}/drivers/video/logo/logo_mac_clut224.ppm
    cp ../gis.ppm ${S}/drivers/video/logo/logo_superh_clut224.ppm
    cp ../gis.ppm ${S}/drivers/video/logo/logo_linux_clut224.ppm
    cp ../gis.ppm ${S}/drivers/video/logo/logo_sun_clut224.ppm
    cp ../gis.ppm ${S}/drivers/video/logo/logo_dec_clut224.ppm
    cp ../gis.ppm ${S}/drivers/video/logo/logo_sgi_clut224.ppm
    cp ../gis.ppm ${S}/drivers/video/logo/logo_parisc_clut224.ppm
    cp ../gis.ppm ${S}/drivers/video/logo/logo_superh_vga16.ppm
    cp ../gis.ppm ${S}/drivers/video/logo/logo_linux_vga16.ppm
    cp ../gis.ppm ${S}/drivers/video/logo/logo_spe_clut224.ppm
    cp ../gis.ppm ${S}/drivers/video/logo/clut_vga16.ppm

}
