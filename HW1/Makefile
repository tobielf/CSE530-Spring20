KDIR:=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux/usr/src/kernel
#KDIR:=/usr/src/linux-headers-4.15.0-76-generic
CC = i586-poky-linux-gcc
#CC = gcc
CROSS_COMPILE = i586-poky-linux-
#CROSS_COMPILE = 

PWD:= $(shell pwd)
VPS:=

ARCH = x86
SROOT=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux/
EXTRA_CFLAGS += -g -Wall
#New kbuild to add -Wall flag.
#ccflags-y += -Wall

APP = rb530
TEST = tester

obj-m:= rb530_drv.o
obj-m+= rbprobe.o
rbprobe-objs := rbprobe-core.o ring_buff.o

all:
	make ARCH=x86 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules
	$(CC) -Wall -o $(APP) main.c -lpthread
	$(CC) -Wall -o $(TEST) tester.c


clean:
	make -C $(KDIR) M=$(PWD) clean
	rm $(APP) $(TEST)

deploy-wifi:
	tar czf programs.tar.gz rb530_drv.ko $(TEST) $(APP) rbprobe.ko
	scp programs.tar.gz root@10.0.1.100:/home/root/
	ssh root@10.0.1.100 'tar xzf programs.tar.gz'

deploy-vps:
	tar czf programs.tar.gz rb530_drv.ko rb530 tester rbprobe.ko
	scp -P 6766 programs.tar.gz root@$(VPS):/root/
	ssh -p 6766 root@$(VPS) '/root/upload.sh'