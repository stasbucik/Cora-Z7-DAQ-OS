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

#
# This file is the daqsrv-udp recipe.
#

SUMMARY = "Simple daqsrv-udp application"
SECTION = "PETALINUX/apps"
LICENSE = "GPLv3+"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-3.0-or-later;md5=1c76c4cc354acaac30ed4d5eefea7245"
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
