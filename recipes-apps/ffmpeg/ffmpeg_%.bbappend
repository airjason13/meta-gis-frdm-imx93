
PACKAGECONFIG[webp] = "--enable-libwebp,--disable-libwebp,libwebp"


PACKAGECONFIG = "avdevice avfilter avcodec avformat swresample swscale postproc avresample \
                   alsa bzlib lzma pic pthreads shared theora zlib webp alsa gpl x264 xcb \
                   ${@bb.utils.contains('AVAILTUNES', 'mips32r2', 'mips32r2', '', d)} \
                   ${@bb.utils.contains('DISTRO_FEATURES', 'x11', 'xv xcb', '', d)}"



