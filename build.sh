#!/bin/bash

TARGET=$1
ROOTFS="bdh_root"

if [ -z "$TARGET" ]; then
    echo "Usage: ./build.sh [phone | laptop]"
    exit 1
fi

echo "==============================================="
echo "  Building BDH Linux for: $TARGET"
echo "==============================================="

echo "[1/4] Preparing Root Filesystem..."
rm -rf $ROOTFS initramfs.cpio.gz bdh-linux.iso iso/
mkdir -p $ROOTFS/{bin,dev,etc,proc,sys,root,lib,home}

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

echo "[3/4] Downloading Kernel, Drivers & BusyBox..."
if [ "$TARGET" == "laptop" ]; then
    wget -q -O bzImage-x86 https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/x86_64/netboot/vmlinuz-lts
    echo "   -> Fetching Kernel Drivers..."
    wget -q -O initramfs-x86.gz https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/x86_64/netboot/initramfs-lts
    mkdir -p temp_mod && cd temp_mod
    gzip -dc ../initramfs-x86.gz | cpio -id --quiet 2>/dev/null
    cd ..
    cp -r temp_mod/lib/modules $ROOTFS/lib/
    rm -rf temp_mod initramfs-x86.gz

    wget -q -O $ROOTFS/bin/busybox https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox
    chmod +x $ROOTFS/bin/busybox
elif [ "$TARGET" == "phone" ]; then
    wget -q -O bzImage-arm https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/aarch64/netboot/vmlinuz-lts
    
    echo "   -> Fetching Kernel Drivers (Network)..."
    wget -q -O initramfs-arm.gz https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/aarch64/netboot/initramfs-lts
    mkdir -p temp_mod && cd temp_mod
    gzip -dc ../initramfs-arm.gz | cpio -id --quiet 2>/dev/null
    cd ..
    cp -r temp_mod/lib/modules $ROOTFS/lib/
    rm -rf temp_mod initramfs-arm.gz
    
    echo "   -> Fetching official static BusyBox from Alpine..."
    proot-distro login alpine -- sh -c "apk add --quiet busybox-static && cp /bin/busybox.static $PWD/$ROOTFS/bin/busybox"
    chmod +x $ROOTFS/bin/busybox
fi

echo "[3.5/4] Adding Custom Apps (BDH Terminal Engine)..."
# --- BDH ENGINE AUTO-BUILD START ---
echo "   -> Auto-compiling BDH Terminal Engine (Static) via Alpine..."
if [ ! -d "$HOME/bdh-terminal-engine" ]; then
    git clone https://github.com/BackendDeveloperHub/bdh-terminal-engine.git $HOME/bdh-terminal-engine
fi

if command -v proot-distro &> /dev/null; then
    proot-distro login alpine -- sh -c "cd $HOME/bdh-terminal-engine && gcc -static -Isrc -D_GNU_SOURCE src/main.c src/engine/*.c src/ui/*.c -o bdh-engine"
else
    cd $HOME/bdh-terminal-engine && gcc -static -Isrc -D_GNU_SOURCE src/main.c src/engine/*.c src/ui/*.c -o bdh-engine
    cd - > /dev/null
fi

mkdir -p custom_apps
cp $HOME/bdh-terminal-engine/bdh-engine custom_apps/
# --- BDH ENGINE AUTO-BUILD END ---

if [ -d "custom_apps" ]; then
    cp -r custom_apps/* $ROOTFS/bin/
    chmod +x $ROOTFS/bin/bpm 2>/dev/null
    chmod +x $ROOTFS/bin/bdh-engine 2>/dev/null
fi

# --- THE MAGIC ROOT FIX IS HERE ---
echo "[4/4] Packing initramfs as TRUE ROOT (UID 0:0)..."
if command -v proot-distro &> /dev/null; then
    proot-distro login alpine -- sh -c "cd $PWD/$ROOTFS && chown -R 0:0 . && chmod 4755 bin/busybox && find . | cpio -ov -H newc | gzip -9 > ../initramfs.cpio.gz" 2>/dev/null
else
    cd $ROOTFS
    chown -R 0:0 . 2>/dev/null
    chmod 4755 bin/busybox 2>/dev/null
    find . | cpio -ov -H newc | gzip -9 > ../initramfs.cpio.gz 2>/dev/null
    cd ..
fi

if [ "$TARGET" == "laptop" ]; then
    echo "Creating Bootable ISO for Laptop..."
    mkdir -p iso/boot/grub
    cp bzImage-x86 iso/boot/
    cp initramfs.cpio.gz iso/boot/
    cat <<EOF > iso/boot/grub/grub.cfg
set timeout=5
set default=0
menuentry "BDH Linux (x86_64)" {
    linux /boot/bzImage-x86 console=tty0 init=/init
    initrd /boot/initramfs.cpio.gz
}
EOF
    grub-mkrescue -o bdh-linux.iso iso/ 2>/dev/null
    echo -e "\n✅ Success! 'bdh-linux.iso' is ready for Ventoy!"
else
    echo -e "\n✅ Success! ARM OS is ready."
    
    # --- AUTO-GENERATE LAUNCHER SCRIPT ---
    echo "   -> Creating launcher script (start_bdh.sh)..."
    cat << 'EOF' > start_bdh.sh
#!/bin/bash
echo "Starting BDH Linux (ARM Environment)..."

if [ ! -f "bdh_disk.img" ]; then
    echo "First boot detected. Creating 512MB Persistence Disk..."
    dd if=/dev/zero of=bdh_disk.img bs=1M count=512 status=none
fi

qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -kernel bzImage-arm -initrd initramfs.cpio.gz -append "console=ttyAMA0 init=/init" -m 256M -netdev user,id=net0 -device virtio-net-pci,netdev=net0 -drive file=bdh_disk.img,format=raw,if=virtio
EOF
    
    chmod +x start_bdh.sh
    echo -e "✅ Launcher ready! Just run: ./start_bdh.sh"
fi
