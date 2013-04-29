#!/usr/bin/python

##Test script for GAVR
###############################################################
## testGAVR.py
## Author: Todd Sukolsky
## Copyright of Todd Sukolsky and Re.Cycle
## Date Created: 2/9/2013
## Last Revised: 4/8/2013
###############################################################
## Description:
##    This module is responisble for communication between the GAVR
## and the BeagleBone
###############################################################
## Revisions:	
##	3/31- Tweaked funciontality to work with current model.
##	4/8-- Edited to match commWAVR parameters that have been
##	      proven to work. Must send time with beginning 'S'
##	      then two characters for each time param, date can be single.
###############################################################
## Changes to be made:
##     
###############################################################

import time
import sys
import os
import serial
import optparse

##OPTPARSER INFO
inputString=''
askUser=False
sendTrips=False

use = "Usage: %prog [options] <arguments>. String entered must have '.' to terminate."
parser = optparse.OptionParser(usage=use)
parser.add_option('-s', dest='inString', help='String to be sent, real mode')
parser.add_option('-t',action="store_true",dest="trips",default=False,help='Send trips to GAVR')
parser.add_option('-D',action="store_true", dest="debug",default=False,help='User String for debug mode')

(options, args) = parser.parse_args()

if options.inString is None and options.debug is False and options.trips is False:
   	askUser=False
	inputString='NONE.'
    	sendTrips=False
elif options.debug is True and options.trips is False and options.inString is None:
    	askUser=True
	sendTrips=False
elif options.debug is False and options.trips is True and options.inString is None:
	askUser=False
	sendTrips=True
elif options.inString is not None:
    	askUser=False
    	inputString=options.inString
else:
	askUser=True
	sendTrips=False
##################################################################
#################### 	Functions 	##########################
##################################################################

#### Setup Connection Routine ####
def setupConnection():
	#Setup communication line with board, Alert the Watchdog we are about to ask for something
	sendInterrupt()
	#Get ACKB
	settingUp=True
	start=time.time()
	while settingUp:
	    if (time.time()-start)<3:		    #Sets timeout to 2 seconds, less than half of actual timeout on AVR
                ack=getString(250.0/1000.0)    
                if (ack=='A.'):
		    return True
	        elif ack!='' or ack!=' ':
		    print '--' + ack + '--'
            else:					#Took to long, chip might be talking to graphics avr, shouldnt be though...wait for timeout to occur then retry
		time.sleep(8)				#Timeout on AVR is 5 seconds
		sendInterrupt()				#send interrupt, reset start for new timeout
		start=time.time()

	return False

#### Send Time Routine ####
def sendTime(theString,stopOnOne):			#stopOnOne==askUser
	#Got ack, time to send time string good or bad
	communicating=True
	start=time.time()
	if stopOnOne:
		oString=theString
	else:
		oString=theString
	sendString(oString)
	ack=''
	while communicating:
		if stopOnOne==False:
			communicating=False
			
		if (time.time()-start)<7 and ack=='':		#longer timeout, only 2 from first + 4 now is full
                        ack=getString(500.0/1000.0)     	#check every 100ms	
			ackCompare='E.'				#NOte this is the testing dummy, shouldn't really need this. If there is an error, exit.
			if (ack==ackCompare):
				print '--'+ack
				return False
				communicating=False
  			elif (ack=='A.'):
				return False

	        else:		#we probably just had a timeout, setup the connection again
			return False	
			communicating=False		#wiping our ass to make sure theres no doo-doo
	return True

#### Send Interrupt Routine ####
def sendInterrupt():
	os.system("echo 1 > /sys/class/gpio/gpio44/value")
	time.sleep(25.0/1000.0)
	os.system("echo 0 > /sys/class/gpio/gpio44/value")
	time.sleep(25.0/1000.0)

#### Send String Routine ####
def sendString(STRING):
	for char in STRING:
		serPort.write(char)
		print char
		time.sleep(250.0/1000.0)
	
#### Get String Routine ####
def getString(waitTime):
	time.sleep(waitTime)
	string=''
	string+=serPort.read(serPort.inWaiting())
	print string
	return string



####################################################################
#####################   Main Program   #############################
####################################################################

#Initializations for the serial port. Bone communicates on UART4, GAVR on UART0
serialPort='/dev/ttyO5'
baudRate=9600

#Declare file i/o variables
logFile='/home/root/Documents/tmp/logs/startupLog.txt'

#logFile='/home/todd/Documents/GitHubProjects/beagle-bone.git/startupLog.txt'		#what we are going to print to for log
try:
    outputSuccess=open(logFile,'a')
except:
    outputSuccess=open(logFile,'w+')

## Set port functions, then open
serPort = serial.Serial(
        port=serialPort,
        baudrate=baudRate,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        bytesize=serial.EIGHTBITS
)
try:
	serPort.open()
except:
	print "Error opening port" + serialPort
	exit()

### MAIN FUNCTIONALITY HERE ###
#initialize boolean values. Will keep trying until ACK is acheived
connectionSet=False
sent=False
setString=''
while not sent:
	while not connectionSet:
		connectionSet=setupConnection()
	if not sent:
		if askUser:
		    setString=raw_input(">>")
		else:
		    setString=inputString
		sent=sendTime(setString,askUser)
		if not sent:
			connectionSet=False
			time.sleep(750.0/1000.0)
# Exit
exit()
