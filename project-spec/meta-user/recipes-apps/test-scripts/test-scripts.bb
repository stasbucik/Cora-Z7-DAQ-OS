SUMMARY = "Test scripts"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://test.py"

RDEPENDS:${PN} += "python3"

do_install() {
             install -d ${D}/usr/share/test-scripts
             install -m 0755 ${WORKDIR}/test.py ${D}/usr/share/test-scripts
}

FILES:${PN} += "/usr/share/test-scripts/test.py"
	
