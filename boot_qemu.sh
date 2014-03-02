#!/bin/bash
cp kry_kern isodir/boot/kry_kern
grub-mkrescue -o krypton.iso isodir
qemu-system-i386 -cdrom krypton.iso


