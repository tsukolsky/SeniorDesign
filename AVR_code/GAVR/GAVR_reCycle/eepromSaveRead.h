/*******************************************************************************\
| eepromSaveRead.h
| Author: Todd Sukolsky
| Initial Build: 1/31/2013
| Last Revised: 3/28/13
| Copyright of Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description:	This header contains the EEPROM memory locations for the variables
|				defined below as well as the routines to save those variables to memory
|				and read them. It also contains the function that is called on the 
|				RTC interrupt that determines whether the EEPROM should be written 
|				to (happens on new day and new hour).
|--------------------------------------------------------------------------------
| Revisions:1/31- Initial build. Just copy and past of stuff from RTC.cpp
|			3/28- Started working on Trip EEPROM storage.
|================================================================================
| *NOTES:
\*******************************************************************************/

using namespace std;

//Declare the global variable that matters
extern myTime currentTime;

//Define EEPROM Block Offset parameters.
#define AVESPEED			0
#define AVEHR				2
#define DISTANCE			4
#define START_DAYS			6
#define START_YEARS			8
#define SECONDS_IN_DAY		10
#define DAYS_ELAPSED		12
#define YEARS_ELAPSED		14
#define SPEED_READINGS_L	16
#define SPEED_READINGS_H	18
#define HR_READINGS_L		20
#define HR_READINGS_H		22

//EEPROM variables
BYTE EEMEM eeHour = 5;
BYTE EEMEM eeMinute = 15;
BYTE EEMEM eeSecond = 10;
BYTE EEMEM eeMonth = 4;
BYTE EEMEM eeDay = 1;
WORD EEMEM eeYear = 2013;

//Could put tripFinished and numberOfTrips in an EEMEM.

/*************************************************************************************************************/
void TripEEPROM(){
	BYTE tripFinished;
	BYTE numberOfTrips;
	
	tripFinished = eeprom_read_byte((BYTE*)1);
	numberOfTrips= eeprom_read_byte((BYTE*)0);	//little endian
	
	if (tripFinished){
		//Start a new trip
	} else {
		//Load the trip data from EEPROM based on the numberOfTrips. If numberOfTrips=3, need to load the third. AVESPEED for that trip is located at 2+(24*2)
		WORD offset=2+((numberOfTrips-1)*24);
		WORD aveSpeed, aveHR, distance,startDays,startYears,secondsInDay,daysElapsed,yearsElapsed,speedReadings_L,speedReadings_H,hrReadings_L,hrReadings_H;
		aveSpeed=eeprom_read_word((WORD*)(offset+AVESPEED));
		aveHR = eeprom_read_word((WORD*)(offset+AVEHR));
		distance=eeprom_read_word((WORD*)(offset+DISTANCE));
		startDays=eeprom_read_word((WORD*)(offset+START_DAYS));
		startYears=eeprom_read_word((WORD*)(offset+START_YEARS));
		secondsInDay=eeprom_read_word((WORD*)(offset+SECONDS_IN_DAY));
		daysElapsed=eeprom_read_word((WORD*)(offset+DAYS_ELAPSED));
		yearsElapsed=eeprom_read_word((WORD*)(offset+YEARS_ELAPSED));
		speedReadings_L=eeprom_read_word((WORD*)(offset+SPEED_READINGS_L));
		speedReadings_H=eeprom_read_word((WORD*)(offset+SPEED_READINGS_H));
		hrReadings_L=eeprom_read_word((WORD*)(offset+HR_READINGS_L));
		hrReadings_H=eeprom_read_word((WORD*)(offset+HR_READINGS_H));
		
		//Set the current trip with these parameters now.
	}//end if-else
}

/*************************************************************************************************************/

void getDateTime_eeprom(BOOL gTime, BOOL gDate){			//get date and time from EEPROM
	cli();
	if (gTime){
		BYTE tempMin, tempSec, tempHour;
		int times=0;
		BOOL notGood=fTrue;
		while(notGood && times<3){
			tempSec = eeprom_read_byte(&eeSecond);
			tempMin = eeprom_read_byte(&eeMinute);
			tempHour = eeprom_read_byte(&eeHour);
			if (tempSec/60==0 && tempMin/60==0 && tempHour/24==0){currentTime.setTime((int)tempHour,(int)tempMin,(int)tempSec); notGood=fFalse;}
			else {times++;}
		}
		if (notGood){currentTime.setTime(1,1,1);}
	}		
	if (gDate){
		BYTE tempDay,tempMonth;
		WORD tempYear;
		int times=0;
		BOOL notGood=fTrue;
		while (notGood && times<3){
			tempDay = eeprom_read_byte(&eeDay);
			tempMonth = eeprom_read_byte(&eeMonth);
			tempYear = eeprom_read_word(&eeYear);
			if (tempDay/31==0 && tempMonth/13==0 && tempYear/10000==0){currentTime.setDate((int)tempMonth,(int)tempDay,(int)tempYear); notGood=fFalse;}
			else {times++;}
		}
		if (notGood){currentTime.setDate(1,1,2001);}	
	}
	sei();
}
/*************************************************************************************************************/

void saveDateTime_eeprom(BOOL sTime, BOOL sDate){
	cli();
	if (sTime){
		BYTE tempSec,tempMin,tempHour;
		tempHour = currentTime.getHours();
		tempMin = currentTime.getMinutes();
		tempSec = currentTime.getSeconds();
		eeprom_write_byte(&eeSecond,tempSec);
		eeprom_write_byte(&eeMinute,tempMin);
		eeprom_write_byte(&eeHour,tempHour);
	}
	if (sDate){
		BYTE tempDay,tempMonth;
		WORD tempYear;
		tempYear = currentTime.getYears();
		tempMonth = currentTime.getMonths();
		tempDay = currentTime.getDays();
		eeprom_write_word(&eeYear,tempYear);
		eeprom_write_byte(&eeMonth,tempMonth);
		eeprom_write_byte(&eeDay,tempDay);
	}
	sei();
}

/*************************************************************************************************************/

/*************************************************************************************************************/