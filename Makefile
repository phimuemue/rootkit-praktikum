obj-m += mod.o
mod-objs := hook_read.o hide_module.o sysmap.o global.o hide_sockets.o hide_files.o hide_processes.o


all: sysmap.h
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules


sysmap.h:
		sh ./create_sysmap.sh

clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
