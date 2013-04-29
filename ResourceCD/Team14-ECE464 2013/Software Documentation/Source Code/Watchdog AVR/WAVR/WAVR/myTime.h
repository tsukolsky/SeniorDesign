/*******************************************************************************\
| myTime.h
| Author: Todd Sukolsky
| Initial Build: 1/29/2013
| Last Revised: 3/4/2013
| Copyright of Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description: This is the Time class that will keep the realTimeClock. it is in 24
|			   hour setting based on EastCoast standard time. This inherits from Date class
|--------------------------------------------------------------------------------
| Revisions:1/29- Initial build.
|				1/30- Added get functions for hour,minute,second. Moved some functions
|					  to private, invoked save Time to EEPROM when a new minute happens and 
|					  save the date when new Day is hit. New day is set before it is saved**
|					  --Need to put saveEEPROm and getEEPROm functions in a header for this 
|						to use along with main, otherwise this has to include main which
|						isnt good.
|				3/4- Added checkValidity() thing to ensure good communiction/error checking.
|================================================================================
| *NOTES:
\*******************************************************************************/

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "myDate.h"

using namespace std;

class myTime : public myDate{
	public:
		myTime();
		myTime(int hour,int minute,int second);
		myTime(int hour,int minute,int second,int month,int day,int year);
		BYTE getSeconds();
		BYTE getMinutes();
		BYTE getHours();
		void setTime(int hour,int minute,int second);
		BOOL setTime(char *newTime);
		void setSecond(int second);
		void addSeconds(int seconds);
		const char * getTime();
		BOOL checkValidity();
		
	private:
		volatile int hour,minute,second;
		char timeString[11];
		void addHours(int hours);
		void addMinutes(int minutes);
		void setHour(int hour);
		void setMinute(int minute);
};

myTime::myTime():myDate(){
	setTime(0,0,0);
}

myTime::myTime(int hour, int minute, int second):myDate(){
	setTime(hour,minute,second);
}

myTime::myTime(int hour,int minute,int second,int month, int day, int year):myDate(month,day,year){
	setTime(hour,minute,second);
}

BYTE myTime::getSeconds(){
	return (BYTE)second;
}

BYTE myTime::getMinutes(){
	return (BYTE)minute;
}

BYTE myTime::getHours(){
	return (BYTE)hour;
}

BOOL myTime::setTime(char *newTime){
	int tempNum[3];
	char currentString[10];
	char tempString[10];
	strcpy(currentString,newTime);
	for (int j=0; j<3; j++){
		for (int i=0; i<2; i++){
				tempString[i]=currentString[i+j*3];
		}
		tempNum[j]=atoi(tempString);
	}
	if (tempNum[0]/24==0 && tempNum[1]/60==0 && tempNum[2]/60==0){
		setTime(tempNum[0],tempNum[1],tempNum[2]);
		return fTrue;
	} 
	return fFalse;
}

void myTime::setTime(int hour,int minute,int second){
	setHour(hour);
	setMinute(minute);
	setSecond(second);
}

void myTime::setHour(int hour){
	if (hour/24 == 0){
		this->hour = hour;
	}	
}

void myTime::setMinute(int minute){
	if (minute/60 == 0){
		this->minute = minute;
	}	
}

void myTime::setSecond(int second){
	if (second/60 == 0){
		this->second = second;
	}	
}

void myTime::addHours(int hours){
	volatile int tempHours = hour + hours;
	hour = tempHours%24;
	if (tempHours/24 >= 1){
		volatile int daysToAdd = tempHours/24;
		addDays(daysToAdd);
	}	
}

void myTime::addMinutes(int minutes){
	volatile int tempMinutes = minute + minutes;				//what the minutes were + added minutes
	minute = tempMinutes%60;
	if (tempMinutes/60 >= 1){
		volatile int hoursToAdd = tempMinutes/60;
		addHours(hoursToAdd);
	}
}

void myTime::addSeconds(int seconds){
	volatile int tempSecond = second + seconds;
	second = (tempSecond)%60;			  //what's left over
	if ((tempSecond)/60 >= 1){
		//There are more than 60 seconds now, find out how many minutes need to be added
		volatile int minutesToAdd = (tempSecond)/60;		  //if seconds = 120, adds two minutes
		addMinutes(minutesToAdd);					  //add to minutes
	}
}

BOOL myTime::checkValidity(){
	BOOL stillValid=fTrue;
	if (second/60==0 && minute/60==0 && hour/24==0){
		if (checkValidityDate()){return fTrue;}
	}
	return fFalse;
}

const char * myTime::getTime(){
	char hourString[3], minuteString[3],secondString[3];
	itoa(hour,hourString,10);
	itoa(minute,minuteString,10);
	itoa(second,secondString,10);
	strcpy(timeString,hourString);
	strcat(timeString,":");
	strcat(timeString,minuteString);
	strcat(timeString,":");
	strcat(timeString,secondString);
	timeString[11] = '\0';
	return timeString;
}