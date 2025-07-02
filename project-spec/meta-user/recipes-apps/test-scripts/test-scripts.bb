#  Copyright 2025, University of Ljubljana
#
#  This file is part of Cora-Z7-DAQ-OS.
#  Cora-Z7-DAQ-OS is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by the Free Software Foundation,
#  either version 3 of the License, or any later version.
#  Cora-Z7-DAQ-OS is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#  You should have received a copy of the GNU General Public License along with Cora-Z7-DAQ-OS.
#  If not, see <https://www.gnu.org/licenses/>.

SUMMARY = "Test scripts"
SECTION = "PETALINUX/apps"
LICENSE = "GPLv3+"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-3.0-or-later;md5=1c76c4cc354acaac30ed4d5eefea7245"

SRC_URI = "file://test-memmap.py \
	file://test-device-file.py"

RDEPENDS:${PN} += "python3"

do_install() {
             install -d ${D}/usr/share/test-scripts
             install -m 0755 ${WORKDIR}/test-memmap.py ${D}/usr/share/test-scripts
             install -m 0755 ${WORKDIR}/test-device-file.py ${D}/usr/share/test-scripts
}

FILES:${PN} += "/usr/share/test-scripts/test-memmap.py"
FILES:${PN} += "/usr/share/test-scripts/test-device-file.py"
	
