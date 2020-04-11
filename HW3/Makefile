KDIR:=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux/usr/src/kernel
#KDIR:=/usr/src/linux-headers-4.15.0-34-generic
CC = i586-poky-linux-gcc
#CC = gcc
CROSS_COMPILE = i586-poky-linux-
#CROSS_COMPILE = 

PWD:= $(shell pwd)

ARCH = x86
SROOT=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux
EXTRA_CFLAGS += -g -Wall

LDLIBS = -L$(SROOT)/usr/lib
CCFLAGS = -I$(SROOT)/usr/include/libnl3

SPI = spi_tester
APP = heart

obj-m = heartbeat.o
heartbeat-objs := heartbeat-core.o

all:
	make ARCH=x86 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules
	$(CC) -Wall -o $(SPI) main.c
	$(CC) -Wall -o $(APP) heartbeat.c $(CCFLAGS) -lnl-genl-3 -lnl-3 -lpthread

clean:
	make -C $(KDIR) M=$(PWD) clean

deploy:
	tar czf programs.tar.gz led.sh spi.sh $(APP) $(TEST) $(SPI) heartbeat.ko
	scp programs.tar.gz root@10.0.1.100:/home/root
	ssh root@10.0.1.100 'tar xzf programs.tar.gz'