#!/bin/bash
set -e

dest1=boot
dest2=rootfs

mkdir $dest1
sudo mkfs.vfat /dev/"$1"1
sudo mount /dev/"$1"1 $dest1
sudo cp images/linux/BOOT.BIN $dest1
sudo cp images/linux/boot.scr $dest1
sudo cp images/linux/image.ub $dest1
sudo umount $dest1
rmdir $dest1

mkdir $dest2
sudo mkfs.ext4 -F /dev/"$1"2
sudo mount /dev/"$1"2 $dest2
sudo cp images/linux/rootfs.tar.gz $dest2
pushd $dest2
sudo tar -xzvf rootfs.tar.gz
sudo rm rootfs.tar.gz
popd
sudo umount $dest2
rmdir $dest2

sudo eject /dev/"$1"
