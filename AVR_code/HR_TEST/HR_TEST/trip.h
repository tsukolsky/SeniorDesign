/*******************************************************************************\
| trip.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 2/26/2013
| Last Revised: 2/26/2013
| Copyright of Todd Sukolsky and BU ECE Senior Design teamp Re.Cycle
|================================================================================
| Description: This is a trip class that is used in the GAVR module. Calls things 
|	       from odometer class.
|--------------------------------------------------------------------------------
| Revisions:  2/26: Initial build
|================================================================================
| *NOTES: There are extra constructors in this file. Please read comments in actual
|	  functions for reasoning and what should be used when. If space is limited,
|	  some constructors can be taken out. Also reference "odometer.h"
\*******************************************************************************/

#include <stdlib.h>
#include "odometer.h"


using namespace std;

class trip: public odometer{
	public:
		trip();
		trip(float wheelSize,int hour, int minute, int second, int day, int month, int year);
		trip(float swapAveSpeed, float swapDistance, float swapCurrentSpeed, unsigned int swapSpeedPoints, float swapWheelSize, int swapHour, int swapMinute, int swapSecond, int swapDay, int swapMonth, int swapYear, float swapAveHR);
		void setTripData(float wheelSize, int hour, int minute, int second, int day, int month, int year, float aveHR);		
		void setStartTime(int hour, int minute, int second);
		void setStartDate(int day, int month, int year);
		void setStartTimeAndDate(int hour, int minute, int second, int day, int month, int year);
		float getAveHR();

	private:
		float aveHR;
		int startHour,startMinute,startSecond,endHour,endMinute,endSecond;
		int startDay,startMonth,startYear,endDay,endMonth,endYear;

};

//Default/no-arg constructor
trip::trip():odometer(){
	startHour=0;
	startMinute=0;
	startSecond=0;
	startDay=0;
	startMonth=0;
	startYear=0;
}

//Constructor for new trip on restart (the trip was over when the device was powered down). Never going to happen in our implentation, but the option is there.
trip::trip(float wheelSize, int hour, int minute, int second, int day, int month, int year):odometer(wheelSize){
	startHour=hour;
	startMinute=minute;
	startSecond=second;
	startDay=day;
	startMonth=month;
	startYear=year;
}

//Constructor for new trip on restart (the trip was killed due to low battery, wasn't finished. Need to reload everything.
trip::trip(float swapAveSpeed, float swapDistance, float swapCurrentSpeed, unsigned int swapSpeedPoints, float swapWheelSize, int swapHour, int swapMinute, int swapSecond, int swapDay, int swapMonth, int swapYear, float swapAveHR):odometer(swapAveSpeed, swapDistance, swapCurrentSpeed, swapWheelSize, swapSpeedPoints){
	
	startHour=swapHour;
	startMinute=swapMinute;
	startSecond=swapSecond;
	startDay=swapDay;
	startMonth=swapMonth;
	startYear=swapYear;
	aveHR=swapAveHR;
} 

//Return the aveHR for storage if needbe.
float trip::getAveHR(){
	return aveHR;
}

//Set the already existing trip with things found in the EEPROM. THis is for a NEW TRIP on start/restart, an existing trip needs to use the constructor below 
void trip::setTripData(float wheelSize, int hour, int minute, int second, int day, int month, int year, float aveHR):odometer(wheelSize){
	startHour=hour;
	startMinute=minute;
	startSecond=second;
	startDay=day;
	startMonth=month;
	startYear=year;
	this->aveHR=aveHR;
}		

//Set the already existing (global) trip with things foundin EEPROM. THis is for a trip that was cutoff due to shutdown and was just reloaded.
void trip::setTripData(float swapAveSpeed, float swapDistance, float swapCurrentSpeed, unsigned int swapSpeedPoints, float swapWheelSize, int swapHour, int swapMinute, int swapSecond, int swapDay, int swapMonth, int swapYear, float swapAveHR):odometer(swapAveSpeed, swapDistance, swapCurrentSpeed, swapWheelSize, swapSpeedPoints):odometer(swapAveSpeed, swapDistance, swapCurrentSpeed,swapSpeedPoints,swapWheelSize){
	
	startHour=swapHour;
	startMinute=swapMinute;
	startSecond=swapSecond;
	startDay=swapDay;
	startMonth=swapMonth;
	startYear=swapYear;
	aveHR=swapAveHR;
} 

void trip::setStartTime(int hour, int minute, int second){
	startHour=hour;
	startMinute=minute;
	startSecond=second;
}

void trip::setStartDate(int day, int month, int year){
	startDay=day;
	startMonth=month;
	startYear=year;
}

void trip::setStartTimeAndDate(int hour, int minute, int second, int day, int month, int year)
	setStartDate(day, month, year);
	setStartTime(hour, minute,second);
}













