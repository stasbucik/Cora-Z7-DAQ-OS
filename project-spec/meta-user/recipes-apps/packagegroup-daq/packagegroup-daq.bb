DESCRIPTION = "Daq debug tools package group"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit packagegroup

PACKAGES = "\
	${PN}-debug-tools \
	"

RDEPENDS:${PN}-debug-tools = "\
	gdb \
	elfutils \
	perf \
	strace \
	procps \
	sysstat"
