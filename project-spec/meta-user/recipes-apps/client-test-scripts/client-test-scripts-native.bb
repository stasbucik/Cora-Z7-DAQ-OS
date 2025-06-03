#
# This file is the client-test-scripts recipe.
#

SUMMARY = "Simple client-test-scripts application"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "git://github.com/lava/matplotlib-cpp;branch=master;protocol=https \
           file://cpp/recv-udp.cpp \
           file://Makefile \
           file://python/recv-udp.py \
           file://python/recv-tcp.py \
		  "

S = "${WORKDIR}"
SRCREV = "ef0383f1315d32e0156335e10b82e90b334f6d9f"

inherit native

DEPENDS        += "boost-native python3-native python3-numpy-native python3-matplotlib-native"
RDEPENDS:${PN} += "boost-native python3-native python3-numpy-native python3-matplotlib-native"

do_compile() {
		 RECIPE_PYTHON_VERSION="$(ls ${STAGING_DIR_NATIVE}/usr/include | grep python)"
		 RECIPE_PYTHON_INCLUDE="${STAGING_DIR_NATIVE}/usr/include/${RECIPE_PYTHON_VERSION}"
		 RECIPE_NUMPY_INCLUDE="${STAGING_DIR_NATIVE}/usr/lib/${RECIPE_PYTHON_VERSION}/site-packages/numpy/core/include"
		 RECIPE_MATPLOTLIBCPP="${S}/git"
		 RECIPE_SYSTEM_INCLUDE="${STAGING_DIR_NATIVE}/usr/include"
         
         #bbwarn "$(ls ${STAGING_DIR_NATIVE}/usr/include/boost)"

	     oe_runmake RECIPE_MATPLOTLIBCPP=${RECIPE_MATPLOTLIBCPP} \
	     	RECIPE_PYTHON_VERSION=${RECIPE_PYTHON_VERSION} \
	     	RECIPE_PYTHON_INCLUDE=${RECIPE_PYTHON_INCLUDE} \
	     	RECIPE_NUMPY_INCLUDE=${RECIPE_NUMPY_INCLUDE} \
	     	RECIPE_SYSTEM_INCLUDE=${RECIPE_SYSTEM_INCLUDE}
}

do_install() {
	     install -d ${D}${bindir}
	     install -m 0755 recv-udp ${D}${bindir}
         install -m 0755 ${WORKDIR}/python/recv-udp.py ${D}${bindir}
         install -m 0755 ${WORKDIR}/python/recv-tcp.py ${D}${bindir}
}

FILES:${PN} += "${bindir}recv-udp.py"
FILES:${PN} += "${bindir}recv-tcp.py"