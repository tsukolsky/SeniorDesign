#!/usr/bin/python

##Test script for GAVR
###############################################################
## ReceiveGAVR.py
## Author: Todd Sukolsky
## Copyright of Todd Sukolsky and Re.Cycle
## Date Created: 4/8/2013
## Last Revised: 4/15/2013
###############################################################
## Description:
##    This module is responisble for communication between the GAVR
## and the BeagleBone for Trip data and delete data's
###############################################################
## Revisions:	
##	4/8- Initial build.
## 	4/15- Added startNewTripGPS, deleteGPStrip, changed name of
##	      deleteTrip to deleteUSBtrip. Now fork/execv functionality
##	      is done using arguments in tuple format. Does IO cahnging and
##	      moving when there is a delete. Can't delete current trip. Can't
## 	      offload the current trip. (2) FInished delete USBtrip file management 
###############################################################
## Changes to be made:
## 
###############################################################

import time
import sys
import os
import serial
import optparse
import glob
import subprocess

serialPort='/dev/ttyO4'
baudRate=9600
USBpath='/media/USB'
#USBpath='/media/SUKOLSKY16G'
VirtualPath='/ReCycle/Trips/'
gpsPath='/ReCycle/gps/'
tripLocation=USBpath+VirtualPath
gpsLocation=USBpath+gpsPath
boneGPSpath='/home/root/Documents/tmp/gps/'
tripToOffload=0				#If offloading, GAVR sends "T<tripNumber>." that its' offloading, matches GPS trip number in path.

#### Send String Routine ####
def sendString(STRING):
#	print STRING
	for char in STRING:
		print "Writing " + char
		serPort.write(char)
		time.sleep(250.0/1000.0)    #25 is good, 10 is too fast, misses it sometimes...
		
#### Get String Routine ####
def getString(waitTime):
	time.sleep(waitTime)
	string=''
	seconds=time.time()
	string=serPort.read(serPort.inWaiting())
	print string
	if string:
		return string
	else:
		exit()

def deleteUSBTrip(whichTrip):
	#Should do file IO and move them all down, as well as GPS data.
	rmCommand='rm '+tripLocation+str(whichTrip)+'.txt'
	os.system(rmCommand)
	rmCommand='rm '+USBpath+gpsPath+str(whichTrip)+'.txt'
	os.system(rmCommand)
	##Move all the trips down
	#See how many trips there are. Number of GPS files are also number of Trips
	filep=gpsLocation+'.info'
	IF=open(filep,'r')
	lines=IF.readlines()
	howManyTrips=int(lines[0].rstrip().strip())
	#Move
	counter=0
	for counter in range(whichTrip+1,howManyTrips+1):
		OLDGF=gpsLocation+str(counter)+'.txt'
		OLDTF=tripLocation+str(counter)+'.txt'
		NEWGF=gpsLocation+str(counter-1)+'.txt'
		NEWTF=tripLocation+str(counter-1)+'.txt'
		mvCommand1='mv '+OLDGF+' '+NEWGF
		mvCommand2='mv '+OLDTF+' '+NEWTF
		os.system(mvCommand1)
		os.system(mvCommand2)
	#Replace number of trips in .info file
	IF.close()
	IF=open(filep,'w')
	IF.write(str(howManyTrips-1))
	IF.close()
	filep=tripLocation+'.info'
	IF=open(filep,'w')
	IF.write(str(howManyTrips-1))
	IF.close()
	
def deleteGPStrip(whichTrip):
	rmCommand='rm '+boneGPSpath+str(whichTrip)+'.txt'
	os.system(rmCommand)
	#Now we need to shift all the files down one. ie, if there were 4 trips and we deleted 1, trip 4 needs to go to 3, 3->2, 2->1
	#See how many trips there are in file before the delete.
	filep=boneGPSpath+'.info'
	IF=open(filep,'r')
	lines=IF.readlines()
	howManyTrips=int(lines[0].rstrip().strip())		#.rstrip gets rid of \n, strip gets rid of whitespace on left and right
	#Number of trips we need to move down is howManyTrips-whichTrip
	counter=0
	for counter in range(whichTrip+1,howManyTrips+1): #if wt=2, hmt=4, moves 3->2, 4->3
		OLDF=boneGPSpath+str(counter)+'.txt'
		NEWF=boneGPSpath+str(counter-1)+'.txt'
		mvCommand='mv '+OLDF+' '+NEWF
		os.system(mvCommand)
		
	#Now that everything has been shifted down, rewreite how many GPS files are actually in this
	IF.close()
	OF=open(filep,'w')
	OF.write(str(howManyTrips-1))
	OF.close()
		
		
def startNewTripGPS(whichTrip):
	##Need to halt myGpsPipe, then move "CURRENT.txt" into the current trip we are working on.
	#First must stop GPS pipe process
	p= subprocess.Popen(['ps','-A'],stdout=subprocess.PIPE)
	out,err=p.communicate()

	for line in out.splitlines():
		if 'myGpsPipe' in line:
			pid=int(line.strip().split(' ')[0])
			print 'myGpsPipe on pid='+str(pid)+',should kill'
			os.kill(pid,9)			#command is "kill -9 pid"
	
	#Process is killed, need to call nmeaLocation.py to parse for time,date,locations.
	os.system('/home/root/Documents/beagle-bone.git/NMEA/nmeaLocation.py')
	#Waits for return, then we can move the CURRENT.txt into the trip folder we want.
	mvCommand='mv '+boneGPSpath+'PARSED.txt '+boneGPSpath+str(whichTrip)+'.txt'
	rmCommand='rm '+boneGPSpath+'CURRENT.txt'
	os.system(mvCommand)
	os.system(rmCommand)
	filep=boneGPSpath+'.info'
	OF=open(filep,'w')
	OF.write(str(whichTrip))		#Which trip actually represents how many trips are there.
	OF.close()						

	#Restart myGpsPipe script.
	pid=os.fork()
	if pid==0:
		os.setpgid(0,0)
		args=['/home/root/Documents/beagle-bone.git/NMEA/myGpsPipe.py','']
		os.execv(args[0],args)
	#Don't wait for it

def writeToUSB(strings):
	tripInfoFile=tripLocation+'.info'
	readFile=open(tripInfoFile,'r')
	lines=readFile.readlines()
	tripNumber=int(lines[0].rstrip())
	readFile.close()
	#We know how many trips there are. We now need to add a new trip
	tripNumber+=1
	writeFile=tripLocation+str(tripNumber)+'.txt'
	OF=open(writeFile,'w')
	for string in strings:
		OF.write(string)
	
	#Put that there is 1 more file in the USB tirp files. This is used for number of GPS files as well.
	echoCommand="echo "+str(tripNumber)+" > " + tripInfoFile
	print 'Echoing '+echoCommand
	os.system(echoCommand)			#update the .info file	
	##Put the GPS file onto the USB.
	cpCommand="cp /home/root/Documents/tmp/gps/"+str(tripToOffload)+".txt "+USBpath+gpsPath+str(tripNumber)+'.txt'
	print 'Copying '+cpCommand
	os.system(cpCommand)
	##UPdate the number of GPS files in the folder
	echoCommand='echo '+str(tripNumber)+' > '+USBpath+gpsPath+'.info'
	print 'Echoing: '+echoCommand
	os.system(echoCommand)



##Function get's called when there is an interrupt from the GAVR. Need to reply with an "A." then get ready to receive
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

### Start functionality here ###
gpsInfoFile=boneGPSpath+'.info'
readFile2=open(gpsInfoFile,'r')
gpsInfoLines=readFile2.readlines()
gpsNumber=int(gpsInfoLines[0].rstrip())
##Set up bools, state for machine, response
communicating=True
flagError=False
state=0
response=''
sendTrips=False
tripStrings=[]


while communicating:
	##INitial ACK back
	if (state==0):
		sendString("A.")
		state=1
	##Wait for a command, once a string is had go to state 2
	elif (state==1):
		response = getString(200.0/1000.0)
		response.strip()
		state=2
	##Evaulate what was sent back.
	elif (state==2): #Only time this sucker doesn't exit is when we are receiving Trip data.
		if (response=='E.'):
			print "State 2 Receive Error: Error receiving on port " + serialPort
			communicating=False
		elif (response.find('NT.')!=-1):
			sendTrips=True							#Need to send them trips, start the transmission to GAVR script.
			sendString('NT.')						#Respond with appropriate ack
			communicating=False
		elif (response.find('DU')!=-1):						#User wants to delete a trip on the USB, see which one then do it
			splitResponse=response.split('.')
			sendString(response)
			deleteUSBTrip(int(splitResponse[0][2:]))			#Call delete trips with the appropriate trip number
			communicating=False
		elif (response[0]=='T'):							#Sending us trip data to put onto USB
			state=3
			tripToOffload=int(response.split('.')[0][1:])			
			sendString('T.')						#ack back that we know the trip is coming.
		elif (response.find('DB')!=-1):	#Delete a GPS trip. File management needed.
			splitResponse=response.split('.')
			deleteGPStrip(int(splitResponse[0][2:]))			#first string has letters, second has .
			sendString(response)
			communicating=False
		elif (response.find('ST.')!=-1):							#Start a new GPS trip
			sendString('ST.')
			startNewTripGPS(gpsNumber+1)					#send the trip number of the last one which is tripNumber+1
			communicating=False
		else:
			sendString("E.")
			print "State 2 Error: Unknown ACK on port " + serialPort
			communicating=False
	##Trip data is being sent, get the string with data. First two characters describe what it is.	
	elif (state==3):
		response=getString(400.0/1000.0)
		state=4
	##If the string was 'D.', we have received all trip data. If not, write to the file
	elif (state==4):
		if (response=='E.'):						#There was an error
			print "State 4 Error: Error receiving trip on port " + serialPort
			communicating=False
			flagError=True
		elif (response.find('D.')!=-1):						#We are done communicating
			communicating=False
			flagError=False
			writeToUSB(tripStrings)
			print "Got done. Exiting."
		else:
			sendString(response[:2]+'.')
			strToFile=response+'\n'
			tripStrings.append(strToFile)				#Added to the file.
			state=3							#Go back to state 3 and get the next field
	else:
		print "Error processing state...out of bounds"

##end while communicating
	
##Now see if we need to send the trips, or delete something.
if sendTrips is True:
	print 'Sending trips'
	pid2=os.fork()
	if pid2==0:		#child
		args=['/home/root/Documents/beagle-bone.git/CommScripts/SendGAVR.py','-t','true','']
		os.execv(args[0],args)
	else:
		os.waitpid(pid2,0)	
	
exit()












