# https://www.linuxjournal.com/content/kbuild-linux-kernel-build-system
ARCH=x86 LOCALVERSION= CROSS_CIMPILE=i586-poky-linux- make -j4
cp arch/x86/boot/bzImage ../galileo-install/
