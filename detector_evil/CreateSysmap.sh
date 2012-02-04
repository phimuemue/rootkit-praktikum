#!/bin/bash
# Script to parse all Symbols with T/t D/d or R/r Type in the System.map file
# and write them to sysmap.h
# symbols in this header-file are named as root_SYMBOLNAME to avoid mixing up
#
# If a symbol occurs more then once in System.Map the prefix will be root_nr_
# where nr is the number of the occurence starting with 1. The first occurence
# will still have the prefix root_
#
# Usage :
#  for the standard System.map file in /boot/System.map-VERSION
#  bash CreateSysmap.sh
#  in case you want to use a different System.map-file
#  bash CreateSysmap.sh #System.map-filename
#
E_WRONG_ARGS=85

echo "Creating sysmap"

sysmap=""
case $# in 
 0)
  sysmap="/boot/System.map-$(uname -r)";;
 1)
  sysmap=$1;;
 *)
  echo "Wrong number of arguments!"
  exit $E_WRONG_ARGS;;
esac

: > sysmap.h

declare -A symbols

{
echo "/**"
echo " * File created by CreateSysmap.h from $sysmap"
echo " * on host $HOSTNAME"
echo " */"
echo ""
echo "#ifndef ROOT_SYS_MAP"
echo "#define ROOT_SYS_MAP"
} >> sysmap.h

echo '#include "sysmap.h"' > sysmap.c

while read address symbol_type symbol_name
do

# ignore symbols with other types than  T/t D/d or R/r
  if [[ "$symbol_type" != [DdRrTt] ]]
  then
    continue
  fi

# replace "." in symbol names with "_" in order to be
# able to use them in c
  if [[ "$symbol_name" == *.* ]]
  then
    symbol_name_old=$symbol_name
    symbol_name=${symbol_name//./_}
  fi

# write the symbol to sysmap.h
# check if the symbol has been already declared
# if yes, then add a number to all further declarations
  sym=${symbols["$symbol_name"]};
  if [ -n "$sym" ]
  then
    echo "void* ROOT_${sym}_$symbol_name = (void*)(0x$address);" >> sysmap.c
    echo "extern void* ROOT_${sym}_$symbol_name;" >> sysmap.h
    (( symbols[$symbol_name]++ ))
  else
    echo "void* ROOT_$symbol_name = (void*)(0x$address);" >> sysmap.c
    echo "extern void* ROOT_$symbol_name;" >> sysmap.h
    symbols[$symbol_name]="1"
  fi

done < $sysmap

{
echo "#endif"
} >>sysmap.h

