LICENSE="CLOSED"
FILESEXTRAPATHS:append := "${THISDIR}/files"

SRC_URI += "file://awe_show.mp4 \
            file://target_lock.mp4 \
            file://GenshinImpact.mp4 \
            file://test-materials \
            "


S = "${WORKDIR}"


do_install() {
	install -d ${D}/root/MediaFiles/
        install -d ${D}/root/MediaFiles/thumbnails
        install -d ${D}/root/MediaFiles/Snapshots
        install -d ${D}/root/MediaFiles/Recordings
        install -d ${D}/root/MediaFiles/Media
        install -d ${D}/root/MediaFiles/Playlists
	cp awe_show.mp4 ${D}/root/MediaFiles/Media  
	cp target_lock.mp4 ${D}/root/MediaFiles/Media 
	cp GenshinImpact.mp4 ${D}/root/MediaFiles/Media  
	install -m 0644 ${S}/test-materials/*.png ${D}/root/MediaFiles/Snapshots/
}

FILES:${PN} += " \
                /root/* \
                "

