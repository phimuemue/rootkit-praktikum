#Makefile for assignment 9
obj-m += detector_mod.o

detector_mod-objs := detector_mod_base.o sysmap.o

all:	sysmap sockethiding
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

sysmap:	CreateSysmap.sh
	bash CreateSysmap.sh

sockethiding: sockethiding_detector.o
	cc -o sockethiding_detector sockethiding_detector.o

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f sockethiding_detector sysmap.h sysmap.c
