SUMMARY = "DAQ UDP server systemd unit setup"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit systemd
inherit features_check
REQUIRED_DISTRO_FEATURES = "systemd"

RDEPENDS:${PN} += "daqsrv-udp "

SRC_URI = "file://daqsrv-udp.service"

do_install() {
             install -d ${D}/etc/systemd/system/
             install -m 0755 ${WORKDIR}/daqsrv-udp.service ${D}/etc/systemd/system/
}

SYSTEMD_SERVICE:${PN} = "daqsrv-udp.service"
FILES:${PN} += "/etc/systemd/system/daqsrv-udp.service"
	
