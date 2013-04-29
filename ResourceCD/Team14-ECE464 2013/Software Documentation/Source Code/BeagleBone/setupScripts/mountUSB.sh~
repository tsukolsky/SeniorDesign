#!/bin/sh

# Description: This bash script searches for a known USB drive and checks to see if it is mounted. If it's not mounted, it will mount it.
# NOTE: You need to run this as root.
# Author: Ben Paolillo
# Date: 2/10/13

# Revision by Todd Sukolsky:4/16/13- Added things from Ben's other scripts to format this in one. Checks for usb, if inserted, raise GPIO and wait for eject. Once eject, lower GPIO and wait for insert.
# Revision by Todd SUkolsky:4/19-13- Debug messages now printed to system log. VIew with dmesg.

clear


#Initialize LASTOUTPUT
LASTOUTPUT=''
MOUNTDIR=/media/USB
initial=`ls /media/USB`
if [ -z "$initial" ]; then
	LASTOUTPUT=`dmesg | tail -f | grep 'sda\|sdb\|sdc' | awk '{print $2}' | cut -d "]" -f 1`
fi

##While true loop. Always do this
while true; do
	##Lower GPIO line
	echo 0 > /sys/class/gpio/gpio27/value
	echo "USB-GPIO line low" > /dev/kmsg

	# Looking for USB drives using the mount cmd.
	# Assuming the drive will be /dev/sda1 or /dev/sdb1 or /dev/sdc1
	while true; do
		sleep 5
		echo "Checking for USB insert" > /dev/kmsg
		IOUTPUT=`dmesg | tail -f | grep 'sda\|sdb\|sdc\|sdd\|sde\|sdf\|sdg' | awk '{print $2}' | cut -d "]" -f 1`
		#If there output of that string is null Or that is the same as last output, there isn't a new USB
		if [ -n "$IOUTPUT" -a "$IOUTPUT" != "$LASTOUTPUT" ]; then
			break
		else
			echo "No difference" > /dev/kmsg
		fi
	done #end blocking while  waiting for new usb
		
	LASTOUTPUT=$IOUTPUT

	MOUNTDIR=/media/USB

	#See which drive partition is was applied under, default is sda1
	if dmesg | tail -f | grep -q "sdb1"; then
		USBNAME=/dev/sdb1
		echo "I think sdb1" > /dev/kmsg
	elif dmesg | tail -f | grep -q "sdc1"; then
		USBNAME=/dev/sdc1
		echo "I think sdc1" > /dev/kmsg
	elif dmesg | tail -f | grep -q "sdd1"; then
		USBNAME=/dev/sdd1
		echo "I think sdd1" > /dev/kmsg
	elif dmesg | tail -f | grep -q "sde1"; then
		USBNAME=/dev/sde1
		echo "I think sde1" > /dev/kmsg
	elif dmesg | tail -f | grep -q "sdf1"; then
		USBNAME=/dev/sdf1
		echo "I think sdf1" > /dev/kmsg
	elif dmesg | tail -f | grep -q "sdg1"; then
		USBNAME=/dev/sdg1
	else
		USBNAME=/dev/sda1
		echo "I think sda1" > /dev/kmsg
	fi

	echo "Mounting $USBNAME to $MOUNTDIR..." > /dev/kmsg
	
	# Creating the dir /media/USB (if it does not exist)
	if [ ! -d $MOUNTDIR ]; then
		mkdir -p $MOUNTDIR
	fi
	
	# Mount the drive
	mount -t vfat $USBNAME $MOUNTDIR
	
	#Raise GPIO line to GAVR
	echo 1 > /sys/class/gpio/gpio27/value
	echo "USB-GPIO line high." > /dev/kmsg 
	
	#Setup the files if they don't already exist
	if [ ! -d $MOUNTDIR/ReCycle ]; then
		mkdir $MOUNTDIR/ReCycle
	fi
	if [ ! -d $MOUNTDIR/ReCycle/Trips ]; then
		mkdir $MOUNTDIR/ReCycle/Trips
		echo 0 > $MOUNTDIR/ReCycle/Trips/.info
	fi
	if [ ! -d $MOUNTDIR/ReCycle/gps ]; then
		mkdir $MOUNTDIR/ReCycle/gps
		echo 0 > $MOUNTDIR/ReCycle/gps/.info
	fi

	#Wait for unmount.
	while true; do
		echo "Checking for unmount" > /dev/kmsg
		sleep 5
		DOUTPUT=`dmesg | tail -f | grep "disconnect" | awk '{print $2}' | cut -d "]" -f 1`
		if [ -n "$DOUTPUT" ]; then
			echo "USB disconnected" > /dev/kmsg
			break
		fi
	done #Done with blocking wait for unmount.
	
done	#Done with outer loop that continuously checks.
		
exit	
	


