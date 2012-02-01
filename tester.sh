#!/bin/bash
for num in {1..10}
do
 echo "Number: $num"
done
for i in {1..100}
do
	echo "Round $i"
 	insmod cool_mod.ko
 	rmmod -w cool_mod
 	echo "completed sucessfully."
done
