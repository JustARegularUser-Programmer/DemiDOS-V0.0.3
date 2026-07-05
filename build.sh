#!/bin/bash
rm kernel.bin
rm DemiDOSPro.iso

echo "1. Boot"
as --32 src/boot.s -o boot.o
#as --32 src/boot_rec.s -o boot_rec.o

echo "2. Kernel"
gcc -m32 -c src/kernel.c -o kernel.o -nostdlib -fno-builtin -fno-stack-protector -fno-pie -mpreferred-stack-boundary=2 -fno-asynchronous-unwind-tables
#gcc -m32 -c src/kernel_rec.c -o kernel_rec.o -nostdlib -fno-builtin -fno-stack-protector -fno-pie -mpreferred-stack-boundary=2 -fno-asynchronous-unwind-tables
gcc -m32 -c src/mousedriver.c -o mousedriver.o -nostdlib -fno-builtin -fno-stack-protector -fno-pie -mpreferred-stack-boundary=2 -fno-asynchronous-unwind-tables
gcc -m32 -c src/networkdriver.c -o networkdriver.o -nostdlib -fno-builtin -fno-stack-protector -fno-pie -mpreferred-stack-boundary=2 -fno-asynchronous-unwind-tables
gcc -m32 -c src/pci.c -o pci.o -nostdlib -fno-builtin -fno-stack-protector -fno-pie -mpreferred-stack-boundary=2 -fno-asynchronous-unwind-tables

ld -m elf_i386 --no-warn-rwx-segments -T src/linker.ld -o kernel.bin boot.o kernel.o mousedriver.o networkdriver.o pci.o
#ld -m elf_i386 --no-warn-rwx-segments -T src/linker.ld boot_rec.o kernel_rec.o -o kernel_rec.bin

echo "3. load"
rm boot.o kernel.o
rm -rf iso
mkdir -p iso/boot/grub
cp kernel.bin iso/boot/
#fcp kernel_rec.bin iso/boot/
cat << "EOF" > iso/boot/grub/grub.cfg
insmod vbe
insmod vga
insmod all_video

set gfxmode=auto
set gfxpayload=keep
set timeout=5
set default=0
menuentry "DemiDOS PRO Normal boot" {
    multiboot /boot/kernel.bin
    boot
}
menuentry "DemiDOS PRO Safe boot" {
    multiboot /boot/kernel.bin -safe
    echo "Loading packages..."
    sleep 9
    echo "Loading Services..."
    sleep 5
    boot
}
menuentry "DemiDOS Shutdown" {
    echo "It's now safe to turn off you PC"
    halt
}
EOF
grub-mkrescue -d /usr/lib/grub/i386-pc -o DemiDOSPro.iso iso
qemu-system-i386 -cdrom DemiDOSPro.iso -rtc base=localtime -device piix3-usb-uhci,id=usb -device usb-tablet,bus=usb.0



