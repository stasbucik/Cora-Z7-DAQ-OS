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

SUMMARY = "DAQ UDP server systemd unit setup"
SECTION = "PETALINUX/apps"
LICENSE = "GPLv3+"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-3.0-or-later;md5=1c76c4cc354acaac30ed4d5eefea7245"

inherit systemd
inherit features_check
REQUIRED_DISTRO_FEATURES = "systemd"

RDEPENDS:${PN} += "daqsrv-udp coreutils "

SRC_URI = "file://daqsrv-udp.service"

do_install() {
             install -d ${D}/etc/systemd/system/
             install -m 0755 ${WORKDIR}/daqsrv-udp.service ${D}/etc/systemd/system/
}

SYSTEMD_SERVICE:${PN} = "daqsrv-udp.service"
FILES:${PN} += "/etc/systemd/system/daqsrv-udp.service"
	
