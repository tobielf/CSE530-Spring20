README
Team1, Xiangyu 1211550272

1. How to make,
Before you start, create a new image.

a. apply the patch file to the old kernel
change directory to where you can see the "kernel" folder, then apply the patch.
➜  CSE530 ls ./ | grep kernel
kernel
➜  CSE530 patch -p1 < new.patch
You should see:
patching file arch/x86/syscalls/syscall_32.tbl
patching file include/linux/dynamic_dump_stack.h
patching file lib/Kconfig.debug
patching file lib/Makefile
patching file lib/dynamic_dump_stack.c

b. compile the new kernel.
ARCH=x86 LOCALVERSION= CROSS_CIMPILE=i586-poky-linux- make -j4
cp arch/x86/boot/bzImage ../galileo-install/

c. load the image to the SD card and reboot the board.

d. compile the user space testing program and the testing driver.
make

2. How to use & sample run.

Insert the testing device driver module into the kernel.
root@quark:~# insmod mt530_drv.ko

You can use other drivers if you want, but you need to recompile the testing program.

Run the testing program.
root@quark:~# ./syscall

And check the output in dmesg.
[  434.402628] Hello world
[  434.405424] Symbol name: invalid_symbol_test
[  434.504405] Invalid symbol name invalid_symbol_test!
[  434.513694] Hello world
[  434.516511] Symbol name: test_data_section_symbol
[  434.717536] Found: test_data_section_symbol b
[  434.722046] test_data_section_symbol Not in text section!
[  434.730985] Hello world
[  434.733785] Symbol name: dev_write
[  434.933756] Found: dev_write t
[  435.038874] Planted kprobe at d2a17020
[  435.044218] kprobe at d2a17020 unregistered
[  435.048749] Goodbye world
[  435.058115] Hello world
[  435.060768] Symbol name: dev_write
[  435.260822] Found: dev_write t
[  435.358592] Planted kprobe at d2a17020
[  435.365608] Goodbye world
[  435.373557] Hello world
[  435.376361] Symbol name: dev_read
[  435.576308] Found: dev_read t
[  435.676019] Planted kprobe at d2a17010
[  435.680031] kprobe dev_read at d2a17010 unregistered
[  435.687312] Hello world
[  435.689806] Symbol name: dev_write
[  435.889644] Found: dev_write t
[  435.988856] Planted kprobe at d2a17020
[  435.994316] CPU: 0 PID: 344 Comm: syscall Tainted: G           O   3.19.8-yocto-standard #25
[  436.002685] Hardware name: Intel Corp. QUARK/GalileoGen2, BIOS 0x01000200 01/01/2014
[  436.002685]  ce60e120 ce60e120 cd797efc c145ba5a cd797f04 c126a1a1 cd797f1c c10a59ca
[  436.002685]  ce60e128 00000246 00000001 d2a17020 cd797f28 c10285ea cd798dc0 cd797f88
[  436.002685]  d27ae264 cd798dc0 00000001 bfeda06c 00000001 d2a17020 cd797f88 cd798dc0
[  436.002685] Call Trace:
[  436.002685]  [<c145ba5a>] dump_stack+0x16/0x18
[  436.002685]  [<c126a1a1>] handler_pre+0x41/0x50
[  436.002685]  [<c10a59ca>] opt_pre_handler+0x3a/0x60
[  436.002685]  [<d2a17020>] ? dev_read+0x10/0x10 [mt530_drv]
[  436.002685]  [<c10285ea>] optimized_callback+0x5a/0x70
[  436.002685]  [<d2a17020>] ? dev_read+0x10/0x10 [mt530_drv]
[  436.002685]  [<c111007b>] ? vwrite+0x1cb/0x1d0
[  436.002685]  [<d2a17021>] ? dev_write+0x1/0x10 [mt530_drv]
[  436.002685]  [<c111f995>] ? vfs_write+0x95/0x1f0
[  436.002685]  [<c11200af>] ? SyS_write+0x3f/0x90
[  436.002685]  [<c145f944>] ? syscall_call+0x7/0x7
[  436.002685] CPU: 0 PID: 344 Comm: syscall Tainted: G           O   3.19.8-yocto-standard #25
[  436.002685] Hardware name: Intel Corp. QUARK/GalileoGen2, BIOS 0x01000200 01/01/2014
[  436.002685]  ce60e5a0 ce60e5a0 cd797efc c145ba5a cd797f04 c126a1a1 cd797f1c c10a59ca
[  436.002685]  ce60e5a8 00000246 00000001 d2a17020 cd797f28 c10285ea cd798dc0 cd797f88
[  436.002685]  d27ae264 cd798dc0 00000001 bfeda06c 00000001 d2a17020 cd797f88 cd798dc0
[  436.002685] Call Trace:
[  436.002685]  [<c145ba5a>] dump_stack+0x16/0x18
[  436.002685]  [<c126a1a1>] handler_pre+0x41/0x50
[  436.002685]  [<c10a59ca>] opt_pre_handler+0x3a/0x60
[  436.002685]  [<d2a17020>] ? dev_read+0x10/0x10 [mt530_drv]
[  436.002685]  [<c10285ea>] optimized_callback+0x5a/0x70
[  436.002685]  [<d2a17020>] ? dev_read+0x10/0x10 [mt530_drv]
[  436.002685]  [<c111007b>] ? vwrite+0x1cb/0x1d0
[  436.002685]  [<d2a17021>] ? dev_write+0x1/0x10 [mt530_drv]
[  436.002685]  [<c111f995>] ? vfs_write+0x95/0x1f0
[  436.002685]  [<c11200af>] ? SyS_write+0x3f/0x90
[  436.002685]  [<c145f944>] ? syscall_call+0x7/0x7
[  436.226145]
[  436.226145] mt530-0 is closing
[  436.231308] kprobe at d2a17020 unregistered
[  436.235536] Goodbye world
[  436.238185] Hello world
[  436.240784] Symbol name: dev_write
[  436.440368] Found: dev_write t
[  436.540805] Planted kprobe at d2a17020
[  436.546146] CPU: 0 PID: 344 Comm: syscall Tainted: G           O   3.19.8-yocto-standard #25
[  436.550049] Hardware name: Intel Corp. QUARK/GalileoGen2, BIOS 0x01000200 01/01/2014
[  436.550049]  ce60e120 ce60e120 cd797efc c145ba5a cd797f04 c126a1a1 cd797f1c c10a59ca
[  436.550049]  ce60e128 00000246 00000001 d2a17020 cd797f28 c10285ea cd798dc0 cd797f88
[  436.550049]  d27ae264 cd798dc0 00000001 bfeda068 00000001 d2a17020 cd797f88 cd798dc0
[  436.550049] Call Trace:
[  436.550049]  [<c145ba5a>] dump_stack+0x16/0x18
[  436.550049]  [<c126a1a1>] handler_pre+0x41/0x50
[  436.550049]  [<c10a59ca>] opt_pre_handler+0x3a/0x60
[  436.550049]  [<d2a17020>] ? dev_read+0x10/0x10 [mt530_drv]
[  436.550049]  [<c10285ea>] optimized_callback+0x5a/0x70
[  436.550049]  [<d2a17020>] ? dev_read+0x10/0x10 [mt530_drv]
[  436.550049]  [<c111007b>] ? vwrite+0x1cb/0x1d0
[  436.550049]  [<d2a17021>] ? dev_write+0x1/0x10 [mt530_drv]
[  436.550049]  [<c111f995>] ? vfs_write+0x95/0x1f0
[  436.550049]  [<c11200af>] ? SyS_write+0x3f/0x90
[  436.550049]  [<c145f944>] ? syscall_call+0x7/0x7
[  436.550049] CPU: 0 PID: 344 Comm: syscall Tainted: G           O   3.19.8-yocto-standard #25
[  436.550049] Hardware name: Intel Corp. QUARK/GalileoGen2, BIOS 0x01000200 01/01/2014
[  436.550049]  ce60e5a0 ce60e5a0 cd797efc c145ba5a cd797f04 c126a1a1 cd797f1c c10a59ca
[  436.550049]  ce60e5a8 00000246 00000001 d2a17020 cd797f28 c10285ea cd798dc0 cd797f88
[  436.550049]  d27ae264 cd798dc0 00000001 bfeda068 00000001 d2a17020 cd797f88 cd798dc0
[  436.550049] Call Trace:
[  436.550049]  [<c145ba5a>] dump_stack+0x16/0x18
[  436.550049]  [<c126a1a1>] handler_pre+0x41/0x50
[  436.550049]  [<c10a59ca>] opt_pre_handler+0x3a/0x60
[  436.550049]  [<d2a17020>] ? dev_read+0x10/0x10 [mt530_drv]
[  436.550049]  [<c10285ea>] optimized_callback+0x5a/0x70
[  436.550049]  [<d2a17020>] ? dev_read+0x10/0x10 [mt530_drv]
[  436.550049]  [<c111007b>] ? vwrite+0x1cb/0x1d0
[  436.550049]  [<d2a17021>] ? dev_write+0x1/0x10 [mt530_drv]
[  436.550049]  [<c111f995>] ? vfs_write+0x95/0x1f0
[  436.550049]  [<c11200af>] ? SyS_write+0x3f/0x90
[  436.550049]  [<c145f944>] ? syscall_call+0x7/0x7
[  436.777249]
[  436.777249] mt530-0 is closing
[  436.782272] kprobe at d2a17020 unregistered
[  436.786701] Goodbye world
[  436.795181] Hello world
[  436.797884] Symbol name: dev_read
[  436.997371] Found: dev_read t
[  437.095657] Planted kprobe at d2a17010
[  446.455236] kprobe dev_write at d2a17020 unregistered
[  446.461951] kprobe dev_read at d2a17010 unregistered
[  446.470015]
[  446.470015] mt530-0 is closing
root@quark:~#


EOF