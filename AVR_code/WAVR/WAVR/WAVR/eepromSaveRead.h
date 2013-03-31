/*******************************************************************************\
| eepromSaveRead.h
| Author: Todd Sukolsky
| Initial Build: 1/31/2013
| Last Revised: 1/31/13
| Copyright of Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description:	This header contains the EEPROM memory locations for the variables
|				defined below as well as the routines to save those variables to memory
|				and read them. It also contains the function that is called on the 
|				RTC interrupt that determines whether the EEPROM should be written 
|				to (happens on new day and new hour).
|--------------------------------------------------------------------------------
| Revisions:1/31- Initial build. Just copy and past of stuff from RTC.cpp
|================================================================================
| *NOTES:
\*******************************************************************************/

#include <avr/eeprom.h>
#include "stdtypes.h"

using namespace std;

//Declare the global variable that matters
extern myTime currentTime;

//EEPROM variables
BYTE EEMEM eeHour = 5;
BYTE EEMEM eeMinute = 31;
BYTE EEMEM eeSecond = 0;
BYTE EEMEM eeMonth = 3;
BYTE EEMEM eeDay = 31;
WORD EEMEM eeYear = 2013;


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

