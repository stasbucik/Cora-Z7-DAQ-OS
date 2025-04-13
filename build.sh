#!/bin/bash
set -e

petalinux-build

latest_file=$(ls -t project-spec/hw-description/*.bit 2>/dev/null | head -n 1)
if [ -n "$latest_file" ]; then
    echo "Latest .bit file: $latest_file"
    petalinux-package --boot --fpga $latest_file --fsbl images/linux/zynq_fsbl.elf --u-boot --force
    petalinux-package wic -w project-spec/meta-user/conf/rootfs.wks --wic-extra-args="-c xz" -o .
else
    echo "No .bit files found in ./project-spec/hw-description/"
fi

