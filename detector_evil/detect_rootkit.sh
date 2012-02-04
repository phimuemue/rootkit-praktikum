#!/bin/bash

ROOTKIT=0
#Check if the current user is root
if (( UID != 0 ))
then
  echo "Need to be root to run the script!"
else 
  echo "Rootkit detector is starting"
fi

#Get id for the tests
CHECKID=`date +%s`

#Insert the detection module
echo "Inserting the detector module"
insmod detector_mod.ko id=$CHECKID

#Get the results of the detection module

#Check if the read system call is hooked
echo -ne "read system call hooked:\t"
read_hooked=`grep $CHECKID"_detector_mod;read;" /var/log/messages | cut -d ";" -f 3`
if [[ $read_hooked == "read_detected" ]]
then
  echo "yes"
  ROOTKIT=1
else
  echo "no"
fi

#Check if modules are hidden
#Get the number of modules in sysfs
SYSFS_MOD=`ls -1 /sys/module/ | wc -l`
#Number of modules reported by the detection module
DETECT_MOD=`grep $CHECKID"_detector_mod;modules;" /var/log/messages | cut -d ";" -f 3`
echo -ne "Modules hidden:\t\t\t"
if (( SYSFS_MOD != DETECT_MOD ))
then
  echo "yes"
  ROOTKIT=1
else
  echo "no"
fi

#Remove the detection module
rmmod detector_mod

#Check if netlink is blocked
echo -ne "Netlink blocked:\t\t"
#Insert tcp_diag module
modprobe tcp_diag
#Check if inet_diag and tcp_diag are loaded
TCP_DIAG=`lsmod | grep tcp_diag | wc -l`
if (( TCP_DIAG == 2 ))
then
  #Try to create netlink socket
  netlink_blocked=`./sockethiding_detector`
  if [[ $netlink_blocked == "detected" ]]
  then
    echo "yes"
    ROOTKIT=1
  else
    echo "no"
  fi
else
  echo "failed (module tcp_diag could not be loaded)"
fi

if (( ROOTKIT == 1 ))
then
  echo "Rootkit has been detected"
else
  echo "No rootkit has been detected"
fi
