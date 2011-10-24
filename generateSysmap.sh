#!/bin.bash
# this beast simply filters all lines of adequate type (D, T, or R), and 
# transforms found lines into void pointers for later usage in C.
grep '.* [DTR] .*' /boot/System.map-$(uname -r) | sed -e 's/\(.*\) \(.*\) \(.*\)/void* ptr_\3 = (void*)0x\1;/' >sysmap.h
