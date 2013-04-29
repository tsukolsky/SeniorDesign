#!/usr/bin/python
###############################################################
## myGpsPipe
## Author: Todd Sukolsky
## Copyright of Todd Sukolsky and Re.Cycle
## Date Created: 2/9/2013
## Last Revised: 4/15/2013
###############################################################
## Description:
##    This module is responisble for piping the raw NMEA strings
## from the GPS to a file in the system that other things can 
## utilize.
##
###############################################################
## Changes to be made:
##	Which files are actually being wrriten to and such
###############################################################
## Revisions:
##	Changed which device is used by default.
##	4/15- Pipe writes strings to "CURRENT.TXT" in a path. It's the
##		  job of the ReceiveGAVR to change the filename to what trip it is,
##		  clear the "CURRENT.TXT" file. 
###############################################################

import time
import os
import serial
import sys
import optparse
import glob

##OPTPARSER INFO
use = "Usage: %prog [options] <arguments>"
parser = optparse.OptionParser(usage=use)
parser.add_option('-b', '--baud', dest='baud', help='Set baud rate', type=int)
parser.add_option('-D', '--device', dest='device',help='Serial Port', type=str)
parser.add_option('-d', '--debug', dest='debug',help='Debug Option')

(options, args) = parser.parse_args()

if options.device is None:
  	options.device='/dev/ttyO2'
if options.baud is None:
    	options.baud=int(9600)
if options.debug is None:
	options.debug=False

###########################################################################################
#########################   	Method for placing Time 	###############################
###########################################################################################
def placeTime(timeString):
	#WRITEFILE='/home/todd/Documents/GitHubProjects/beagle-bone.git/NMEA/initTime.txt'
	WRITEFILE='/home/root/Documents/tmp/gps/savedTimeFile.txt'
	
	#Parse the time, then alert the sendTime script that it needs to execute.
	try:
		logFile=open(WRITEFILE,'a')	#append as a log
	except:
		exit()
		
	#Split the string
	fields=timeString.split(',')
	if (timeString.find('GPRMC') != -1) and (fields[2] == 'A'):	#found it, valid, save latest data
		time=fields[1]
		date=fields[9]			
		dmy=date[2:4]+','+date[:2]+',20'+date[4:6]
		hms=str(int(time[:2])-4)+':'+time[2:4]+':'+time[4:6]		#converts from UTC to EST
		
		#Initiate program for communication with the Watchdog_AVR with the new time.
		sendString='S'+hms+'/'+dmy+'.'		
		logFile.write(sendString+'\n')
		print 'Time and date is '+sendString+', forking to call SendWAVR'
		pid=os.fork()
		if pid==0:
			args=['/home/root/Documents/beagle-bone.git/CommScripts/SendWAVR','-s',sendString,'']
			os.execv(args[0],args)
		else:
			os.waitpid(pid,0)

		return False
 	else:
		logFile.write('NONE\n')	#don't need to initiate sending routine
		return True

	writefile.close()

###########################################################################################
#########################   End-Method for placing Time 	###########################
###########################################################################################



###########################################################################################
#########################   	   Main Program			###########################
###########################################################################################
#Find out how many files already exist.
gpsPath='/home/root/Documents/tmp/gps/'

#Declare where the NMEA strings are going to be written
nmeaFile=gpsPath+'CURRENT.txt'
#nmeaFile='/home/todd/Documents/GitHubProjects/beagle-bone.git/NMEA/raw_strings_dev.txt'
try:
	hnmeaFile=open(nmeaFile,'a')	#we are appending to this file just in case there was a restart. if fails, opens it as new since it wasn't an append.
except:
	hnmeaFile=open(nmeaFile,'w')

## Open serial connection that is going to be used for GPS
UART_PORT=str(options.device)
baud=options.baud
GPSport = serial.Serial(
        port=UART_PORT,
        baudrate=baud,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        bytesize=serial.EIGHTBITS
)

try:
	GPSport.open()
	if options.debug:
		print 'Port ' + UART_PORT + ' is open.'
except:
	print 'Error opening port: ' + UART_PORT
	exit()

## Serial port is open, time to start streaming
## Note: NMEA strings terminated by '\n'
currentString=''
stringsWrittenTime=0
stringsWrittenThisSection=0
noError=True
getTime=True
timeSlept=0
char=''
while noError:
	if GPSport.inWaiting() > 0:
		char=GPSport.read(1)	##read one character at a time. as long as this system runs faster than GPS, does fine
		#New if with that last read	
		if char=='\n':
			#New, valid, line- add to string then print to file
			currentString += char
			if options.debug:
				print currentString
				
			#Write to the file with all GPS strings
			hnmeaFile.write(currentString) 			#newline is already appended on end
			stringsWrittenThisSection += 1
			#This section will check to see if string is valid for time output to AVR. If we are in a 40 second increment of the loop and the time is not set, we should send the current string to the function and wait to see if it was a valid. If it was valid the function returns False and we no longer go through this loop; if invalid returns True and we will try another few strings ONLY for this 40 second increment. If it is true, it will not go on the next 40 second increment, but only after a half hour
			if timeSlept%5==0 and getTime:			#if in first 40 seconds, look for real string
				getTime=placeTime(currentString)	#this should really be if there is a GPMRC, who cares
				
			#Null the current string so that the next one can be received correctly	
			currentString=''						
		elif char!='\00' and char != ' ':			#no new line, just another character, add to string
			currentString+= char	
		
		#If 15 strings were writte, take a break for a short time. Ends up with about 30% duty cycle.
		if stringsWrittenThisSection >= 15:
			time.sleep(4)
			timeSlept+=1 
	                stringsWrittenThisSection=0

		#If we've slept 225 times, reset it first off and then enable the pipe to send a time to the WAVR
        if timeSlept >= 225:  #450 represents every hour
			timeSlept=0
			getTime=True		

exit()
