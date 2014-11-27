#!/bin/zsh

qemu-system-i386 \
	-kernel bzImage \
	-hda image \
	-append "console=ttyS0 root=/dev/sda rw panic=1" \
	-nographic \
	-no-reboot
