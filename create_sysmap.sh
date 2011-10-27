#!/bin/sh

# go through output of /proc/kallsyms line by line (this is the default behaviour of awk) and 
# split the line by the default field separator (whitespace). then, look for all entries with T, D or R as the second field
# and create the corresponding entry which is writen to sysmpa.h

awk -- '($2 ~ /[TtDdRr]/) && (schonda[$3] != 1) && ($3 !~ /\./) {
	print "void* ptr_"$3" = (void*) 0x"$1"; // "$0
	schonda[$3] = 1
}' /boot/System.map-$(uname -r) > sysmap.h
