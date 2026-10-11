#!/bin/bash
# install-boot.sh -- run the graphical installer in QEMU, end to end, and check what it wrote.
#
#   tools/testing/install-boot.sh [ide|ahci|nvme|all]            (default: ide)
#
# For each disk bus:
#   1. boot a copy of os.iso whose boot menu starts on the "Install" entry, with a blank disk
#      attached, and drive the wizard with the keyboard (Enter = Next, Space ticks the
#      "I understand" box on the review page) -- screenshot at every page;
#   2. wait for the installer to say it is done (it prints to the serial console);
#   3. check the disk from the host: the GPT with sfdisk, the FAT volume with fsck.fat, and
#      that the kernel image and the RAM disk on it are byte for byte the ones in the tree;
#   4. boot that disk *alone* under SeaBIOS (and, for ide, under OVMF/UEFI), log into the
#      shell over the serial console and `ls /hda` -- the first boot of the installed system.
#
# Needs qemu-system-i386/-x86_64, socat, sfdisk, fsck.fat, mtools, grub-mkrescue and an
# os.iso built with `make iso`. Results (logs, screenshots, disk images) in $OUT.
set -u
cd "$(dirname "$0")/../.."
OUT=${OUT:-/tmp/install-boot}
mkdir -p "$OUT"
KVM=${KVM:--enable-kvm}
DISK_SIZE=${DISK_SIZE:-1G}
INSTALL_WAIT=${INSTALL_WAIT:-300}       # seconds the install may take
BOOT_WAIT=${BOOT_WAIT:-14}              # seconds from power-on until the installer is up
OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS=/usr/share/OVMF/OVMF_VARS_4M.fd

for t in qemu-system-i386 qemu-system-x86_64 socat sfdisk fsck.fat mcopy grub-mkrescue; do
    command -v "$t" >/dev/null || { echo "install-boot: need $t"; exit 1; }
done
for f in os.iso payload.img initrd.img kernel.elf; do
    [ -f "$f" ] || { echo "install-boot: no $f -- run 'make iso' first"; exit 1; }
done

pass=0; fail=0
ok()   { echo "  ok    $*"; pass=$((pass + 1)); }
bad()  { echo "  FAIL  $*"; fail=$((fail + 1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1"; fi; }

# An ISO whose menu starts on the installer (entry 1) and does not wait, so a script need
# not catch GRUB's countdown. The kernel, RAM disk and payload are the real ones.
make_install_iso() {
    local d="$OUT/isotree"
    rm -rf "$d"; mkdir -p "$d/boot/grub"
    for f in kernel.elf initrd.img payload.img; do ln -f "iso/boot/$f" "$d/boot/$f" 2>/dev/null || cp "iso/boot/$f" "$d/boot/$f"; done
    sed -e 's/^set default=0/set default=1/' -e 's/^set timeout=.*/set timeout=1/' \
        arch/x86/boot/grub/grub.cfg > "$d/boot/grub/grub.cfg"
    grub-mkrescue -o "$OUT/install.iso" "$d" >/dev/null 2>&1
}

# qemu_disk <bus> <image>: the machine and disk arguments for a bus
disk_args() {
    case $1 in
    ide)  echo "-machine pc -drive file=$2,format=raw,if=ide,index=0" ;;
    ahci) echo "-machine q35 -drive id=tgt,file=$2,format=raw,if=none -device ich9-ahci,id=ahci -device ide-hd,drive=tgt,bus=ahci.0" ;;
    nvme) echo "-machine pc -drive id=tgt,file=$2,format=raw,if=none -device nvme,drive=tgt,serial=koppios0" ;;
    esac
}

# cdrom_args <bus>: where the installation medium goes, so it never takes the disk's place
cdrom_args() {
    case $1 in
    ahci) echo "-drive id=cd0,file=$OUT/install.iso,format=raw,if=none,media=cdrom -device ide-cd,drive=cd0,bus=ahci.1 -boot d" ;;
    *)    echo "-drive file=$OUT/install.iso,if=ide,index=2,media=cdrom -boot d" ;;
    esac
}

mon() { echo "$2" | socat - "unix-connect:$1" >/dev/null 2>&1; }
key() { mon "$1" "sendkey $2"; sleep "${3:-1}"; }
shot() { mon "$1" "screendump $2.ppm"; sleep 1; command -v pnmtopng >/dev/null && pnmtopng "$2.ppm" > "$2.png" 2>/dev/null && rm -f "$2.ppm"; }

run_install() {
    local bus=$1 d="$OUT/$1"
    rm -rf "$d"; mkdir -p "$d"
    qemu-img create -f raw "$d/disk.img" "$DISK_SIZE" >/dev/null
    echo "=== install on $bus"

    # shellcheck disable=SC2046
    qemu-system-i386 $(disk_args "$bus" "$d/disk.img") $(cdrom_args "$bus") \
        -vga virtio -m 512M -smp 2 -no-reboot $KVM -rtc base=localtime,clock=vm \
        -usb -device usb-kbd,port=1 -device usb-hub,port=2 -device usb-mouse,port=2.1 \
        -display none -serial "file:$d/serial.log" -monitor "unix:$d/mon.sock,server,nowait" &
    local q=$!
    sleep "$BOOT_WAIT"

    local m="$d/mon.sock"
    shot "$m" "$d/1-welcome"
    key "$m" ret                 # Welcome -> Disk
    shot "$m" "$d/2-disk"
    key "$m" ret                 # Disk -> Settings
    shot "$m" "$d/3-settings"
    key "$m" ret                 # Settings -> Review
    shot "$m" "$d/4-review"
    key "$m" spc                 # tick "I understand"
    shot "$m" "$d/4b-review-ticked"
    key "$m" ret 3               # Install
    shot "$m" "$d/5-installing"

    local waited=0
    while ! grep -q "install: \(done\|FAILED\)" "$d/serial.log" 2>/dev/null; do
        sleep 2; waited=$((waited + 2))
        [ $waited -ge "$INSTALL_WAIT" ] && break
        kill -0 $q 2>/dev/null || break
    done
    sleep 2
    shot "$m" "$d/6-finish"
    grep "install:" "$d/serial.log" | sed 's/\x1b\[[0-9;]*[a-zA-Z]//g'
    check "$bus: installer reported success" "grep -q 'install: done' '$d/serial.log'"
    check "$bus: the kernel mounted the new volume" "grep -q 'install: done.*the kernel mounted' '$d/serial.log'"

    key "$m" ret 3               # Restart -> QEMU exits (-no-reboot)
    for _ in 1 2 3 4 5; do kill -0 $q 2>/dev/null || break; sleep 1; done
    if kill -0 $q 2>/dev/null; then bad "$bus: Restart did not reset the machine"; mon "$m" quit; kill $q 2>/dev/null
    else ok "$bus: Restart reset the machine"; fi
    wait $q 2>/dev/null
}

check_disk() {
    local bus=$1 d="$OUT/$1" img="$OUT/$1/disk.img"
    echo "--- the disk, from the host ($bus)"
    local table; table=$(sfdisk -d "$img" 2>&1)
    check "$bus: GPT" "echo \"\$table\" | grep -q 'label: gpt'"
    check "$bus: BIOS boot partition at 2048" "echo \"\$table\" | grep -q 'start= *2048, size= *2048, type=21686148-6449-6E6F-744E-656564454649'"
    check "$bus: EFI System partition at 4096" "echo \"\$table\" | grep -q 'start= *4096.*type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B'"
    sfdisk --verify "$img" >"$d/sfdisk-verify.log" 2>&1
    check "$bus: sfdisk --verify is clean" "grep -q 'No errors' '$d/sfdisk-verify.log'"

    local start size
    start=$(echo "$table" | sed -n 's/.*img2 : start= *\([0-9]*\),.*/\1/p;s/.*disk.img2 : start= *\([0-9]*\),.*/\1/p' | head -1)
    size=$(echo "$table" | sed -n 's/.*img2 : start= *[0-9]*, size= *\([0-9]*\),.*/\1/p' | head -1)
    dd if="$img" of="$d/p2.img" bs=512 skip="$start" count="$size" status=none
    fsck.fat -n "$d/p2.img" >"$d/fsck.log" 2>&1
    check "$bus: fsck.fat is clean" "! grep -qi 'error\|differ\|invalid\|dirty' '$d/fsck.log'"
    local off=$((start * 512))
    mcopy -i "$img@@$off" ::/boot/kernel.elf "$d/kernel.elf" 2>/dev/null
    mcopy -i "$img@@$off" ::/boot/initrd.img "$d/initrd.img" 2>/dev/null
    mcopy -i "$img@@$off" ::/boot/grub/grub.cfg "$d/grub.cfg" 2>/dev/null
    mcopy -i "$img@@$off" ::/EFI/BOOT/BOOTX64.EFI "$d/BOOTX64.EFI" 2>/dev/null
    check "$bus: /boot/kernel.elf is the kernel" "cmp -s '$d/kernel.elf' kernel.elf"
    check "$bus: /boot/initrd.img is the RAM disk" "cmp -s '$d/initrd.img' initrd.img"
    check "$bus: /boot/grub/grub.cfg has no installer entry" "[ -s '$d/grub.cfg' ] && ! grep -q 'payload' '$d/grub.cfg' && grep -q 'multiboot2 /boot/kernel.elf' '$d/grub.cfg'"
    check "$bus: /EFI/BOOT/BOOTX64.EFI present" "[ -s '$d/BOOTX64.EFI' ]"
}

# boot_installed <name> <bus> <bios|uefi>: start the installed disk alone and use its shell.
boot_installed() {
    local bus=$1 fw=$2 d="$OUT/$1" tag="$1-$2"
    echo "--- first boot of the installed disk ($bus, $fw)"
    rm -f "$d/$fw.sock" "$d/$fw.ser" "$d/$fw.log" "$d/$fw.in"
    local machine=(qemu-system-i386)
    local extra=()
    if [ "$fw" = uefi ]; then
        cp -f "$OVMF_VARS" "$d/uefi-vars.fd"
        machine=(qemu-system-x86_64)
        extra=(-drive if=pflash,format=raw,unit=0,readonly=on,file="$OVMF_CODE" -drive if=pflash,format=raw,unit=1,file="$d/uefi-vars.fd")
    fi
    # shellcheck disable=SC2046
    "${machine[@]}" "${extra[@]}" $(disk_args "$bus" "$d/disk.img") \
        -vga virtio -m 1G -smp 2 -no-reboot $KVM -rtc base=localtime,clock=vm \
        -usb -device usb-kbd,port=1 -device usb-hub,port=2 -device usb-mouse,port=2.1 \
        -display none -serial "unix:$d/$fw.ser,server,nowait" -monitor "unix:$d/$fw.sock,server,nowait" &
    local q=$!
    sleep 3
    mkfifo "$d/$fw.in"
    socat "unix-connect:$d/$fw.ser" - < "$d/$fw.in" > "$d/$fw.log" 2>/dev/null &
    local sp=$!
    exec 8>"$d/$fw.in"
    sleep "$BOOT_WAIT"
    printf 'lsblk\r' >&8; sleep 2
    printf 'ls /hda\r' >&8; sleep 3
    shot "$d/$fw.sock" "$d/boot-$fw"
    mon "$d/$fw.sock" quit
    exec 8>&-
    kill $q $sp 2>/dev/null; wait $q $sp 2>/dev/null
    sed 's/\x1b\[[0-9;]*[a-zA-Z]//g; s/\x1b[()][A-Z0-9]//g' "$d/$fw.log" | tr -d '\r' > "$d/$fw.txt"
    check "$tag: GRUB started the kernel from the disk" "grep -q 'initrd: .*RAM disk mounted as rd' '$d/$fw.txt'"
    check "$tag: lsblk shows the disk as a mounted target" "grep -q 'hda .*mounted target' '$d/$fw.txt'"
    check "$tag: ls /hda lists the volume's directories" "grep -qi 'boot' '$d/$fw.txt' && grep -qi 'efi' '$d/$fw.txt'"
}

make_install_iso

buses=${1:-ide}
[ "$buses" = all ] && buses="ide ahci nvme"
for bus in $buses; do
    run_install "$bus"
    check_disk "$bus"
    boot_installed "$bus" bios
    [ "$bus" = ide ] && boot_installed "$bus" uefi
done

echo
echo "install-boot: $pass passed, $fail failed  (logs and screenshots in $OUT)"
[ "$fail" -eq 0 ]
