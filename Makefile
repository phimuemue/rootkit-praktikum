obj-m += mod.o

all: sysmap.h testprogramm
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules


sysmap.h:
		sh ./generateSysmap.sh

testprogramm: testprogramm.c
		gcc -o testprogramm testprogramm.c

clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
