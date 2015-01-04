#!/bin/zsh

if [ "$1" -eq 1 ]
then 
	emu=qemu-system-i386
else 
	emu=./qemu-2.1.2/i386-softmmu/qemu-system-i386
fi

if [ "$1" -eq 2 ]
then 
	sudo mount -o loop,rw -t ext2 image mnt2/
	sudo cp custom_device/test_pci/test_pci_driver.ko mnt2/drivers
	sudo cp user_program/test_pci/test_pci_user mnt2/drivers
	sudo umount mnt2
fi

echo $emu

$emu \
	-kernel bzImage \
	-hda image \
	-append "console=ttyS0 root=/dev/sda rw panic=1" \
	-nographic \
	-no-reboot \
	-device test_pci
