LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://awe_show.mp4 \
            file://target_lock.mp4 \
            file://GenshinImpact.mp4 \
            "


S = "${WORKDIR}"


do_install() {
	install -d ${D}/root/Videos/
        install -d ${D}/root/Videos/thumbnails
        install -d ${D}/root/Videos/.thumbnails
	cp awe_show.mp4 ${D}/root/Videos  
	cp target_lock.mp4 ${D}/root/Videos  
	cp GenshinImpact.mp4 ${D}/root/Videos  

}

FILES:${PN} += " \
                /root/* \
                "

