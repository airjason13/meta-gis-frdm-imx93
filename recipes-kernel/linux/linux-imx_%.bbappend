FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://imx93-gis-frdm-dsi.dts \
            file://waveshare-5inch-dsi-lcd-b.cfg \
	    file://0001-Add-raspberrypi-7inch-panel-desc-and-mode.patch \           
	    file://0002-Add-panel-and-reg_bridge-in-imx93.dtsi.patch \
	    file://imx93-gis-frdm.dts \
	    file://gis.ppm \
            file://panel-jbd-jbd4020-left.c \
            file://panel-jbd-jbd4020-right.c \
            file://panel-jbd-jbd4020.h \
            file://panel-jbd-jbd4020.cfg \
            file://panel-jbd4040.cfg \
            file://i2c-jbd4020.c \
            file://i2c-jbd4040.c \
            file://0003-panel-kconfig-jbd4020.patch \
            file://0003-panel-makefile-jbd4020.patch \
            file://0004-i2c-kconfig-jbd4020.patch \
            file://0004-i2c-makefile-jbd4020.patch \
            file://0005-add-jbd4020-i2c-kconfig.patch \
            file://0005-add-jbd4020-i2c-Makefile.patch \
            file://0006-add-jbd4040-default-m.patch \
            file://0007-add-jbd4040-desc-in-panel-simple.patch \
            file://imx93-gis-frdm-jbd4020.dts \
            file://imx93-gis-sdmb-jbd4020.dts \
            file://imx93-gis-sdmb-jbd4040.dts \
            file://imx93-gis-sdmb-jbd4040-test.dts \
            file://imx93-gis-frdm-jbd4040.dts \
            file://imx93-gis-frdm-jbd4040-160.dts \
            file://imx93-gis-sdmb.dts \
            file://imx93-gis-frdm-test-jbd4040.dts \
	    "


KERNEL_DEVICETREE:append = " freescale/imx93-gis-frdm-dsi.dtb \
                             freescale/imx93-gis-frdm.dtb \
                             freescale/imx93-gis-frdm-jbd4020.dtb \ 
                             freescale/imx93-gis-sdmb-jbd4020.dtb \ 
                             freescale/imx93-gis-sdmb-jbd4040.dtb \ 
                             freescale/imx93-gis-sdmb-jbd4040-test.dtb \ 
                             freescale/imx93-gis-frdm-jbd4040.dtb \ 
                             freescale/imx93-gis-frdm-jbd4040-160.dtb \ 
                             freescale/imx93-gis-sdmb.dtb \
                            "

KERNEL_CONFIG_FRAGMENTS += "panel-jbd-jbd4020.cfg \
                            panel-jbd4040.cfg \
                           "

do_patch:append() {
    install -Dm0644 ${WORKDIR}/imx93-gis-frdm-dsi.dts \
        ${S}/arch/arm64/boot/dts/freescale/imx93-gis-frdm-dsi.dts
    install -Dm0644 ${WORKDIR}/imx93-gis-frdm.dts \
        ${S}/arch/arm64/boot/dts/freescale/imx93-gis-frdm.dts
    install -Dm0644 ${WORKDIR}/imx93-gis-sdmb.dts \
        ${S}/arch/arm64/boot/dts/freescale/imx93-gis-sdmb.dts
    install -Dm0644 ${WORKDIR}/imx93-gis-frdm-jbd4020.dts \
        ${S}/arch/arm64/boot/dts/freescale/imx93-gis-frdm-jbd4020.dts
    install -Dm0644 ${WORKDIR}/imx93-gis-sdmb-jbd4020.dts \
        ${S}/arch/arm64/boot/dts/freescale/imx93-gis-sdmb-jbd4020.dts
    install -Dm0644 ${WORKDIR}/imx93-gis-sdmb-jbd4040.dts \
        ${S}/arch/arm64/boot/dts/freescale/imx93-gis-sdmb-jbd4040.dts
    install -Dm0644 ${WORKDIR}/imx93-gis-sdmb-jbd4040-test.dts \
        ${S}/arch/arm64/boot/dts/freescale/imx93-gis-sdmb-jbd4040-test.dts
    install -Dm0644 ${WORKDIR}/imx93-gis-frdm-jbd4040.dts \
        ${S}/arch/arm64/boot/dts/freescale/imx93-gis-frdm-jbd4040.dts
    install -Dm0644 ${WORKDIR}/imx93-gis-frdm-test-jbd4040.dts \
        ${S}/arch/arm64/boot/dts/freescale/imx93-gis-frdm-test-jbd4040.dts
    install -Dm0644 ${WORKDIR}/imx93-gis-frdm-jbd4040-160.dts \
        ${S}/arch/arm64/boot/dts/freescale/imx93-gis-frdm-jbd4040-160.dts
    install -m 0644 ${WORKDIR}/panel-jbd-jbd4020-left.c ${S}/drivers/gpu/drm/panel/panel-jbd-jbd4020-left.c
    install -m 0644 ${WORKDIR}/panel-jbd-jbd4020-right.c ${S}/drivers/gpu/drm/panel/panel-jbd-jbd4020-right.c
    install -m 0644 ${WORKDIR}/panel-jbd-jbd4020.h ${S}/drivers/gpu/drm/panel/panel-jbd-jbd4020.h
    install -m 0644 ${WORKDIR}/i2c-jbd4020.c ${S}/drivers/i2c/busses/i2c-jbd4020.c
    install -m 0644 ${WORKDIR}/i2c-jbd4040.c ${S}/drivers/i2c/busses/i2c-jbd4040.c
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
