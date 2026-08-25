#!/bin/bash

TARGET=$1
ROOTFS="bdh_root"

if [ -z "$TARGET" ]; then
    echo "Usage: ./build.sh [phone | laptop]"
    exit 1
fi

echo "==============================================="
# --- OS NAME CHANGED HERE ---
echo "  Building BDH Linux for: $TARGET"
echo "==============================================="

echo "[1/4] Preparing Root Filesystem..."
# --- ISO NAME CHANGED HERE ---
rm -rf $ROOTFS initramfs.cpio.gz bdh-linux.iso iso/
mkdir -p $ROOTFS/{bin,dev,etc,proc,sys,root}

echo "[2/4] Compiling bdh_init.c..."
if command -v proot-distro &> /dev/null; then
    echo "   -> Termux detected! Compiling via Alpine automatically..."
    proot-distro login alpine -- sh -c "cd $PWD && gcc -static bdh_init.c -o $ROOTFS/init"
else
    gcc -static bdh_init.c -o $ROOTFS/init
fi

if [ $? -ne 0 ]; then
    echo "Error: Compilation failed!"
    exit 1
fi

echo "[3/4] Downloading Kernel and BusyBox..."
if [ "$TARGET" == "laptop" ]; then
    wget -q -O bzImage-x86 https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/x86_64/netboot/vmlinuz-lts
    wget -q -O $ROOTFS/bin/busybox https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox
    chmod +x $ROOTFS/bin/busybox
elif [ "$TARGET" == "phone" ]; then
    wget -q -O bzImage-arm https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/aarch64/netboot/vmlinuz-lts
    
    # --- NEW ROBUST METHOD FOR ARM BUSYBOX ---
    echo "   -> Fetching official static BusyBox from Alpine..."
    proot-distro login alpine -- sh -c "apk add --quiet busybox-static && cp /bin/busybox.static $PWD/$ROOTFS/bin/busybox"
    chmod +x $ROOTFS/bin/busybox
else
    echo "Error: Invalid target! Use 'phone' or 'laptop'."
    exit 1
fi

# --- NEW STEP: Add Custom Apps ---
echo "[3.5/4] Adding Custom Apps (BDH Terminal Engine)..."
if [ -d "custom_apps" ]; then
    cp -r custom_apps/* $ROOTFS/bin/
fi
# ---------------------------------

echo "[4/4] Packing initramfs..."
cd $ROOTFS
find . | cpio -ov -H newc | gzip -9 > ../initramfs.cpio.gz 2>/dev/null
cd ..

if [ "$TARGET" == "laptop" ]; then
    echo "Creating Bootable ISO for Laptop..."
    mkdir -p iso/boot/grub
    cp bzImage-x86 iso/boot/
    cp initramfs.cpio.gz iso/boot/
    cat <<EOF > iso/boot/grub/grub.cfg
set timeout=5
set default=0
# --- GRUB MENU ENTRY CHANGED HERE ---
menuentry "BDH Linux (x86_64)" {
    linux /boot/bzImage-x86 console=tty0 init=/init
    initrd /boot/initramfs.cpio.gz
}
EOF
    # --- ISO OUTPUT NAME CHANGED HERE ---
    grub-mkrescue -o bdh-linux.iso iso/ 2>/dev/null
    echo -e "\n✅ Success! 'bdh-linux.iso' is ready for Ventoy!"
else
    echo -e "\n✅ Success! ARM OS is ready."
    # --- QEMU NETWORK FLAGS ADDED HERE ---
    echo "Run: qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -kernel bzImage-arm -initrd initramfs.cpio.gz -append \"console=ttyAMA0 init=/init\" -m 256M -netdev user,id=net0 -device virtio-net-device,netdev=net0"
fi
