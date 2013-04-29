#!/usr/bin/python
###############################################################
## nmeaParser.py
## Author: Todd Sukolsky
## Copyright of Todd Sukolsky
## Date Created: 2/9/2013
## Last Revised: 3/31/2013
###############################################################
## Description:
## This script is the foundation for a NMEA string parser
## The script, initially, will pipe data from a USB to 
## a temp file, then look at what is in the temp file for 
## certain NMEA strings. Note the kinds of strings below.
## 
## The final implemention of this parser will be deployed
## onto the BeagleBone where GPS data will be streamed in 
## via UART4 or 5 whcih will be used for GPS location 
## tracking and setting up the time.
##
## This file reads in NMEA data from a file and puts it into a different
## file with actual locations
###############################################################
## Revisions:
##	3/31- Changed string to catch from 'GGA' to 'GMRMC' because
##	      it has the date it it as well, along with LONG and LAT
###############################################################
## Kinds of NMEA (2.0) strings: (more info at http://www.gpsinformation.org/dale/nmea.htm)
## GPAPB: Auto Pilot B <not used>
## GPBOD: Bearing <not used>
## GPGGA: Fix data <not used>
## GPGLL: Lat/Lon data <not used>
## GPGSA: Overall satellite reception data <not used>
## GPSGSV: Detailed satellite data <not used>
## GPRMB:  Minimum data required when following route <not used>
## GPRMC: Minimum data recommended <not used>
## GPRTE: Route data <used>
## GPWPL: Waypoint data <not use>
###############################################################

READFILE = '/home/root/Documents/tmp/gps/CURRENT.txt'
WRITEFILE='/home/root/Documents/tmp/gps/PARSED.txt'

#read the file
infile = open(READFILE,'r')
writefile = open(WRITEFILE,'w')
#writefile.write('TIME\t\tDATE\t\tLAT\t\tLOG\n\n')

#Fin string storing locations, GPGGA
for lines in infile.readlines():
	itsHere=lines.find('GPRMC')
	if (itsHere != -1):	#found it
		try:
			splitLine=lines.split(',')
			#As long as there isn't a NOFIX, manipulate data and place
			if (splitLine[2] == 'A'):
				time=splitLine[1]
				date=splitLine[9]			
				dmy=date[2:4]+','+date[:2]+',20'+date[4:6]
				hms=str(int(time[:2])-5)+':'+time[2:4]+':'+time[4:6]		#converts from UTC to EST
				#print 'Time:'+hms + ', Date:'+dmy
				lat=splitLine[3]
				#print lat[:2]+'.'+lat[2:4] + '/'+lat[5:]
				latstring=str(float(lat[:2])+float(lat[2:])/60)+splitLine[4]
				log=splitLine[5]
				#print lat+'...'+log			
				logstring=str(float(log[:3])+float(log[3:])/60)+splitLine[6]
				writefile.write(hms+'\t'+dmy+'\t'+latstring+'\t'+logstring+'\n')
		except:
			print 'Bad string'

writefile.close()



















