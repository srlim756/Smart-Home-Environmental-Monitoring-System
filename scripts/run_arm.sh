#!/bin/bash
# run_arm.sh -- QEMU ARM 全系统启动脚本
cd ~/qemu-arm

qemu-system-arm \
  -machine virt,gic-version=2 \
  -cpu cortex-a15 \
  -m 256 \
  -kernel ~/qemu-arm/vmlinuz-arm \
  -initrd ~/qemu-arm/initrd-arm \
  -drive file=~/qemu-arm/rootfs.ext4,format=raw,id=rootfs \
  -device virtio-blk-device,drive=rootfs \
  -device virtio-net-device,netdev=net0 \
  -netdev user,id=net0,hostfwd=tcp::8080-:8080 \
  -append "root=/dev/vda console=ttyAMA0" \
  -nographic -serial mon:stdio
