obj-m += draco_module.o
CFLAGS_draco_module.o := -O2
KDIR= /lib/modules/$(shell uname -r)/build
all:
		$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
		rm -rf *.o *.ko *.mod.* .c* .t*
