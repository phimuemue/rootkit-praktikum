obj-m += mod.o

all: sysmap.h
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

sysmap.h:
		sh ./generateSysmap.sh

clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
