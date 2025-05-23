#
# This file is the client-test-scripts recipe.
#

SUMMARY = "Simple client-test-scripts application"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://cpp/recv-udp.cpp \
           file://Makefile \
           file://python/recv-udp.py \
           file://python/recv-tcp.py \
		  "

S = "${WORKDIR}"

inherit native

DEPENDS        += "boost-native"
RDEPENDS:${PN} += "boost-native python3-native"

do_compile() {
	     oe_runmake
}

do_install() {
	     install -d ${D}${bindir}
	     install -m 0755 recv-udp ${D}${bindir}
         install -m 0755 ${WORKDIR}/python/recv-udp.py ${D}${bindir}
         install -m 0755 ${WORKDIR}/python/recv-tcp.py ${D}${bindir}
}

FILES:${PN} += "${bindir}recv-udp.py"
FILES:${PN} += "${bindir}recv-tcp.py"