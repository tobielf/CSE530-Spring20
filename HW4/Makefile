KDIR:=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux/usr/src/kernel
#KDIR:=/usr/src/linux-headers-4.15.0-39-generic
CC = i586-poky-linux-gcc
#CC = gcc
CROSS_COMPILE = i586-poky-linux-
#CROSS_COMPILE = 

PWD:= $(shell pwd)

ARCH = x86
SROOT=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux/
EXTRA_CFLAGS += -g -Wall -DNORMAL_MODULE=1

APP = syscall
TEST = tester

obj-m:= mt530_drv.o

all:
	make ARCH=x86 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules
	$(CC) -Wall -o $(APP) sys_test.c -lpthread
	$(CC) -Wall -o $(TEST) tester.c -lpthread

clean:
	make -C $(KDIR) M=$(PWD) clean

deploy:
	tar czf programs.tar.gz mt530_drv.ko mprobe.ko $(APP) $(TEST)
	scp programs.tar.gz root@10.0.1.100:/home/root/mywork
	ssh root@10.0.1.100 'tar xzf ./mywork/programs.tar.gz'
