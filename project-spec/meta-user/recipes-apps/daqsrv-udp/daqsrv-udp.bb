#
# This file is the daqsrv-udp recipe.
#

SUMMARY = "Simple daqsrv-udp application"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://daqsrv-udp.cpp \
           file://Makefile \
		  "

S = "${WORKDIR}"

RDEPENDS:${PN} += "boost daqdrv"
DEPENDS        += "boost"

do_compile() {
	     oe_runmake
}

do_install() {
	     install -d ${D}${bindir}
	     install -m 0755 daqsrv-udp ${D}${bindir}
}
