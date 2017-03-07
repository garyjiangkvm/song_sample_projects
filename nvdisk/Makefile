#KERNELDIR ?= /root/dev/hisi/linux-3.0.y
KERNELDIR ?= /root/dev/hisi3535/linux-3.4.y_rgmii_rgmii
PWD := $(shell pwd)

obj-m := nvd.o
nvd-objs := nv_proc.o remap.o netlink.o nvdisk.o
all:
	$(MAKE) -C ${KERNELDIR}  ARCH=arm CROSS_COMPILE=arm-hisiv200-linux- M=$(PWD) modules
	arm-hisiv200-linux-gcc -o demo ntlk_demo.c
	rm -f *.o *.mod.c .*.cmd sysenter.h Module.symvers
clean:
	rm -f *.o *.ko *.mod.c .*.cmd sysenter.h Module.symvers
	make -C /lib/modules/$(shell uname -r)/build SUBDIRS=$(PWD) clean
