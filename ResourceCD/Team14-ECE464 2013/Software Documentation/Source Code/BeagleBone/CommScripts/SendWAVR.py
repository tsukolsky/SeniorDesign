#!/usr/bin/python

##Test script for softSetup
#!/usr/bin/python
###############################################################
## setup.py
## Author: Todd Sukolsky
## Copyright of Todd Sukolsky and Re.Cycle
## Date Created: 2/9/2013
## Last Revised: 4/8/2013
###############################################################
## Description:
##    This module is responisble for setting the Watchdog AVR's
## time in the case that there is a valid time stamp from the 
## NMEA strings of the GPS. Date is not preserved and must be
## user provided in the case of a complete system halt. This
## module will establish a connection, send the string that is
## stored in a remote file to the watchdog avr. It will then exit.
##
###############################################################
## Revision:
##	3/31- Changed functionality to match the board we have and
##	      that time and date are both sent.
##  4/8-- Tweaked for it to work standalone on Bone.
###############################################################
## Changes to be made:
##     Probably going to add an outer loop for the send function;
## Every 30 seconds, if no time is set, loop through the function
## looking for valid GPS strings. Once one is good, then proceed.
## This will mean we can integrate initTime.py with setup.py
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

use = "Usage: %prog [options] <arguments>. String entered must have '.' to terminate."
parser = optparse.OptionParser(usage=use)
parser.add_option('-s', dest='inString', help='String to be sent, real mode')
parser.add_option('-D',action="store_true", dest="debug",default=False,help='User String for debug mode')

(options, args) = parser.parse_args()

if options.inString is None and options.debug is False:
    askUser=False
    inputString='NONE.'
elif options.debug is True:
    askUser=True
else:
    askUser=False
    inputString=options.inString

##################################################################
#################### 	Functions 	##########################
##################################################################

#### Setup Connection Routine ####
def setupConnection():
	#Setup communication line with board, Alert the Watchdog we are about to ask for something
	sendInterrupt()
	#Get ACKT
	settingUp=True
	start=time.time()
	ack=''
	while settingUp:
	    if (time.time()-start)<3:		    #Sets timeout to 2 seconds, less than half of actual timeout on AVR
                ack=getString(250.0/1000.0)    
                if (ack=='A.'):
		    return True
	        elif ack!='' or ack!=' ':
		    print '--' + ack + '--'
            else:					#Took to long, chip might be talking to graphics avr, shouldnt be though...wait for timeout to occur then retry
		time.sleep(6)				#Timeout on AVR is 5 seconds
		sendInterrupt()				#send interrupt, reset start for new timeout
		start=time.time()

	return False

#### Send Time Routine ####
def sendTime(theString,stopOnOne):				#stopOnOne==askUser-->if true, don't keep trying to send, always stop after one 								#transmission
	#Got ack, time to send time string good or bad
	communicating=True
	start=time.time()
	if stopOnOne:
		oString=theString	
	else:
		oString=theString					#string we are trying to send.
	sendString(oString)
	ack=''
	while communicating:
		if stopOnOne==False:
			communicating=False
			
		if (time.time()-start)<7 and ack=='':		#longer timeout, only 2 from first + 4 now is full
                        ack=getString(500.0/1000.0)     	#check every 500ms	
			ackCompare='A'+oString[1:]		#this changed to oString[1:]--4/8
			if (ack==ackCompare):
				print '--'+ack
				communicating=False
			elif (ack=='B.'):
				## Check the hour, minute, second to make sure they are correct.
				hour=int(oString[1:3])
				minute=int(oString[4:6])
				second=int(oString[7:9])
				##Assume date is correct. Could add string split and parse if necessary.
				
				if (hour/24==0) and (minute/60==0) and (second/60==0):
					return False			##restarts transmission
				else:
					communicating=False
					return True
                        elif (ack=='A.'):
				return False
			elif (ack=='E.'):
				return False

	        else:		#we probably just had a timeout, setup the connection again
			return False	
			communicating=False		#wiping our ass to make sure theres no doo-doo
	return True

#### Send Interrupt Routine ####
def sendInterrupt():
	os.system("echo \"high\" > /sys/class/gpio/gpio38/direction")
	time.sleep(25.0/1000.00)
	os.system("echo \"low\" > /sys/class/gpio/gpio38/direction")	
	time.sleep(25.0/1000.00)

#### Send String Routine ####
def sendString(STRING):
	for char in STRING:
		print "Writing " + char
		serPort.write(char)
		time.sleep(250.0/1000.0)    #25 is good, 10 is too fast, misses it sometimes...
	
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

#Initializations for the serial port for WAVR, on UART1 for bone, UART0 for WAVR
serialPort='/dev/ttyO1'
baudRate=9600

#Declare file i/o variables
logFile='/home/root/Documents/tmp/logs/startupLog.txt'

#logFile='/home/todd/Documents/GitHubProjects/beagle-bone.git/initialSetupScripts/startupLog.txt'		#what we are going to print to for log

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

#Write to a log that I am done
outputSuccess.write("init_time...DONE\n")
exit()
