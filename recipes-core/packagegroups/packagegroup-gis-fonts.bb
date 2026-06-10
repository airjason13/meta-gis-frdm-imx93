SUMMARY = "All fonts packages "
PR = "r5"


inherit packagegroup features_check


RDEPENDS:${PN} = " \
        fontconfig \
	source-han-sans-cn-fonts \
	source-han-sans-jp-fonts \
	source-han-sans-kr-fonts \
	source-han-sans-tw-fonts \
        ttf-wqy-zenhei \
"
