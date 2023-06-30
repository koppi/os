#!/usr/bin/env bash

LO=`sudo losetup -f`

MNT=/mnt/floppy
IMG=floppy.img

#qemu-img create $IMG 1.44M
/sbin/mkfs.fat $IMG

sudo mkdir -p $MNT
sudo losetup $LO $IMG
sudo mount $LO $MNT -t msdos -o "fat=12"

sudo rm -f $MNT/hello
sudo cp apps/hello/hello $MNT/hello
sudo rm -f $MNT/tst
sudo cp apps/01/01 $MNT/tst
sudo cp mouse.bmp $MNT/mouse.bmp

sudo umount $MNT
sudo losetup -d $LO
sudo rm -rf $MNT
