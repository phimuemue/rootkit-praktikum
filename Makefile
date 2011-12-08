obj-m += mod.o
mod-objs := hook_read.o sysmap.o global.o



all: sysmap.h testprogramm
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

sysmap.h: create_sysmap.sh
		sh ./create_sysmap.sh

testprogramm: testprogramm.c
		gcc -o testprogramm testprogramm.c

clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
