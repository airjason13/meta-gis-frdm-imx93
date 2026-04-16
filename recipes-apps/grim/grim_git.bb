SUMMARY = "Image Grabber For Wayland"
DESCRIPTION = "Grab images from a Wayland compositor. Works great with slurp and with sway"
HOMEPAGE = "https://github.com/emersion/grim"
BUGTRACKER = "https://github.com/emersion/grim/issues"
SECTION = "graphics"
LICENSE = "MIT"

LIC_FILES_CHKSUM = "file://LICENSE;md5=e316e9609dd7672b87ff25b46b2cf3e1"

REQUIRED_DISTRO_FEATURES = "wayland"

DEPENDS += " \
	pixman \
	libpng \
	jpeg \	
	wayland \
	wayland-native \
	wayland-protocols \
"

SRC_URI = "git://git.sr.ht/~emersion/grim;protocol=https;branch=master"

S = "${WORKDIR}/git"
PV = "1.4.1"
SRCREV = "7ba46364ab95141c79e0e18093aa66597256182c"

inherit meson pkgconfig features_check

EXTRA_OEMESON = "--buildtype release"

BBCLASSEXTEND = ""


