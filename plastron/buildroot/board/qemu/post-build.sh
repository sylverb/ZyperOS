#!/bin/sh
# After board/px2/post-build.sh, which QEMU's image shares (ZyperOS's seed
# version, the licenses; not its SSH key, below): the Pixel 2's start
# scripts for parts the virt machine doesn't have. S01governor sets the
# CPU's clock governor (QEMU has no cpufreq) and S40usbgadget makes the USB
# cable a network (QEMU's is virtio-net); both come in with the Pixel 2's
# overlay.
set -eu
rm -f "$TARGET_DIR/etc/init.d/S01governor" "$TARGET_DIR/etc/init.d/S40usbgadget"

# No SSH key in the image, a development build's either: root's key comes
# from QEMU at every boot (rootfs-overlay/etc/init.d/S49sshkey, from
# ZyperOS's tools/qemu-run.sh), into /run/ssh-root, where /root/.ssh points.
# So every QEMU image lets in whoever starts it and nobody else, and one can
# be published (ZyperOS's tools/qemu-image.sh). local/authorized_keys, which
# board/px2/post-build.sh put in, goes.
rm -rf "$TARGET_DIR/root/.ssh"
ln -s /run/ssh-root "$TARGET_DIR/root/.ssh"
