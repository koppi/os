#!/usr/bin/env bash

LO=`sudo losetup -f`

MNT=/mnt/hda
IMG=hda.img

qemu-img create $IMG 5M
/sbin/mkfs.fat $IMG

sudo mkdir -p $MNT
sudo losetup $LO $IMG
sudo mount $LO $MNT -t msdos
sudo rm -f $MNT/hello
sudo cp apps/hello/hello $MNT/hello
sudo umount $MNT
sudo losetup -d $LO
sudo rm -rf $MNT
