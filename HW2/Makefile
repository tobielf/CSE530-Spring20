KDIR:=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux/usr/src/kernel
#KDIR:=/usr/src/linux-headers-4.15.0-34-generic
CC = i586-poky-linux-gcc
#CC = gcc
CROSS_COMPILE = i586-poky-linux-
#CROSS_COMPILE = 

PWD:= $(shell pwd)

ARCH = x86
SROOT=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux/
#EXTRA_CFLAGS += -g -Wall -DNORMAL_MODULE=1
EXTRA_CFLAGS += -g -Wall

APP = hcsr04
TEST = tester

obj-m:= hcsr04.o
hcsr04-objs := hcsr04-core.o ring_buff.o hcsr_config.o hcsr_sysfs.o
obj-m+= hcsr-dev.o
hcsr-dev-objs := hcsr_device.o

all:
	make ARCH=x86 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules
	$(CC) -Wall -o $(APP) main.c -lpthread
	$(CC) -Wall -o $(TEST) tester.c


clean:
	make -C $(KDIR) M=$(PWD) clean

