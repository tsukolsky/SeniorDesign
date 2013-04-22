/*******************************************************************************\
| eepromSaveRead.h
| Author: Todd Sukolsky
| Initial Build: 1/31/2013
| Last Revised: 4/21/13
| Copyright of Todd Sukolsky and Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description:	This header contains the EEPROM memory locations for the variables
|				defined below as well as the routines to save those variables to memory
|				and read them. It also contains the function that is called on the 
|				RTC interrupt that determines whether the EEPROM should be written 
|				to (happens on new day and new hour).
|--------------------------------------------------------------------------------
| Revisions:1/31- Initial build. Just copy and past of stuff from RTC.cpp
|			3/28- Started working on Trip EEPROM storage.
|			4/7-  Changed implementation of EEPROM storage. See note 1. Made functions
|				  that check the eeprom trip data, store given how many trips there are,
|				  and read given which thing they want to be read. (2) Made DeleteTrip which uses 
|				  MoveTrip functionality. Changed "tripsFinished" and "numberofTrips" variables
|				  to static EEMEM locations. Found, by looking at "*.elf" file the way EEPROM
|				  is set up. 
|			4/18- Fixed StartupTripEEPROM(). When a testing started and was killed before a save,
|				  the EEPROM tripFinished BYTE was 0, but there was no wheelSize so nothing worked.
|				  Now on it accurately starts a new trip if the number of trips is still less than 1,
|				  signalling nothing was actually saved in the EEPROM.
|			4/21- Slight tweak to offsets and how they function. Everyting working now with saving.
|================================================================================
| *NOTES: (1) Floats are 4 BYTES, need to change this functionality to accurately get
|			  ave hr, wheel size, distance, ave speed. Offset is going to grow and needs
|			  to be tweaked.
|			--ERROR, dwords are 4 bytes, floats are 32 bits = 4BYTES. http://deans-avr-tutorials.googlecode.com/svn/trunk/EEPROM/Output/EEPROM.pdf
|		  (1b) The eeprom write/reads can be templatized so that we input a value, size of,
|				and then the function will write the correct things. http://arduino.cc/forum/index.php/topic,41497.0.html
|				--Resolved: This actually won't work, needs to be more specific. We can implement a funciton
|							that writes floats though so it can be reused in all cases.
|		  (2) Use "eeprom_UPDATE_<type>" for all writes, preserves EEPROM and if the data inthe block is the same,
|			  doesn't write preserving the data/lifespan of the EEPROM (100,000 reads/writes).
\*******************************************************************************/

using namespace std;

//Declare the global variable that matters
extern myTime currentTime;
extern trip globalTrip;
extern WORD numberOfGAVRtrips;

#define FINISHED			1	
#define UNFINISHED			0
//Define EEPROM Block Offset parameters.
#define INITIAL_OFFSET		20				//Should be 10, but just in case.
#define BLOCK_SIZE			34				//Data spans 34 bytes
//Actual offsets now
#define AVESPEED			0
#define AVEHR				4
#define DISTANCE			8
#define START_DAYS			12
#define START_YEAR			14
#define MINUTES_ELAPSED		16
#define DAYS_ELAPSED		18
#define YEARS_ELAPSED		20
#define SPEED_READINGS_L	22
#define SPEED_READINGS_H	24
#define HR_READINGS_L		26
#define HR_READINGS_H		28
#define WHEEL_SIZE			30		//goes to {33,30}

//EEPROM variables, allocated at location 0 and then up from there. hour=0, minute=1, second=2, month=3, day=5, year=6-7=> total offset is 9 or 10, going for 20 just in case. Covering the buttocks
BYTE EEMEM eeHour = 5;						//location 0
BYTE EEMEM eeMinute = 15;					//location 1
BYTE EEMEM eeSecond = 10;					//location 2
BYTE EEMEM eeMonth = 4;						//location 3
BYTE EEMEM eeDay = 1;						//location 4
WORD EEMEM eeYear = 2013;					//location {6,5}
BYTE EEMEM eeTripFinished = 1;				//location 7 
BYTE EEMEM eeNumberOfTrips = 1;				//location 8. Always one trip...
/*************************************************************************************************************/
void MoveTripDown(BYTE whichTrip){
	//First, get the trip that we are moving
	BYTE offset=INITIAL_OFFSET+(whichTrip-1)*BLOCK_SIZE;
	WORD startDays,startYear,minutesElapsed,daysElapsed,yearsElapsed,speedReadings_L,speedReadings_H,hrReadings_L,hrReadings_H;
	float aveSpeed=eeprom_read_float((float*)(offset+AVESPEED));
	float aveHR=eeprom_read_float((float*)(offset+AVEHR));
	float distance=eeprom_read_float((float*)(offset+DISTANCE));
	startDays=eeprom_read_word((WORD*)(offset+START_DAYS));
	startYear=eeprom_read_word((WORD*)(offset+START_YEAR));
	minutesElapsed=eeprom_read_word((WORD*)(offset+MINUTES_ELAPSED));
	daysElapsed=eeprom_read_word((WORD*)(offset+DAYS_ELAPSED));
	yearsElapsed=eeprom_read_word((WORD*)(offset+YEARS_ELAPSED));
	speedReadings_L=eeprom_read_word((WORD*)(offset+SPEED_READINGS_L));
	speedReadings_H=eeprom_read_word((WORD*)(offset+SPEED_READINGS_H));	//2^16-1=65535, so 65535*this number + SPEED_READINGS_LOW
	hrReadings_L=eeprom_read_word((WORD*)(offset+HR_READINGS_L));
	hrReadings_H=eeprom_read_word((WORD*)(offset+HR_READINGS_H));
	float wheelSize=eeprom_read_float((float*)(offset+WHEEL_SIZE));
	
	//Now save that data in the trip below that one.
	offset=INITIAL_OFFSET+(whichTrip-2)*BLOCK_SIZE;
	eeprom_update_float((float*)(offset+AVESPEED),aveSpeed);
	eeprom_update_float((float*)(offset+AVEHR),aveHR);
	eeprom_update_float((float*)(offset+DISTANCE),distance);
	eeprom_update_word((WORD *)(offset+START_DAYS),startDays);
	eeprom_update_word((WORD *)(offset+START_YEAR),startYear);
	eeprom_update_word((WORD *)(offset+MINUTES_ELAPSED),minutesElapsed);
	eeprom_update_word((WORD *)(offset+DAYS_ELAPSED),daysElapsed);
	eeprom_update_word((WORD *)(offset+YEARS_ELAPSED),yearsElapsed);
	eeprom_update_word((WORD *)(offset+SPEED_READINGS_L),speedReadings_L);
	eeprom_update_word((WORD *)(offset+SPEED_READINGS_H),speedReadings_H);
	eeprom_update_word((WORD *)(offset+HR_READINGS_L),hrReadings_L);
	eeprom_update_word((WORD *)(offset+HR_READINGS_H),hrReadings_H);
	eeprom_update_float((float*)(offset+WHEEL_SIZE),wheelSize);
}
/*************************************************************************************************************/
void DeleteTrip(BYTE whichTrip){
	//Need to shift all trips down by one to that location.
	BYTE numberOfTrips=eeprom_read_byte(&eeNumberOfTrips);
	if (numberOfTrips!=numberOfGAVRtrips){
		numberOfGAVRtrips=numberOfTrips;
	}
	for (int i=whichTrip+1; i<=numberOfTrips; i++){
		MoveTripDown(i);
	}
	numberOfTrips--;
	numberOfGAVRtrips--;
	eeprom_write_byte(&eeNumberOfTrips,numberOfTrips);		//store number of trips back now.
}
/*************************************************************************************************************/
void EndTripEEPROM(){	
	BYTE numberOfTrips=eeprom_read_byte(&eeNumberOfTrips);								//If there is one trip, then the offset should be 1 to get to where new data is stored,\
																						offset is new value of numberOfTrips since the location we are writing two is one above what it should be.
	numberOfTrips++;
	WORD offset=INITIAL_OFFSET+((numberOfTrips-2)*BLOCK_SIZE);	
	eeprom_update_byte(&eeNumberOfTrips,numberOfTrips);									//Put the new amount of trips in the correct location
	eeprom_update_byte(&eeTripFinished,FINISHED);										//say that the trip is done with a 1(True)
	//Now get the necessary data and store in EEPROM.
	eeprom_update_float((float*)(offset+AVESPEED),(float)globalTrip.getAverageSpeed());
	eeprom_update_float((float*)(offset+AVEHR),(float)globalTrip.getAveHR());
	eeprom_update_float((float*)(offset+DISTANCE),(float)globalTrip.getDistance());
	eeprom_update_word((WORD *)(offset+START_DAYS),(WORD)globalTrip.getStartDays());
	eeprom_update_word((WORD *)(offset+START_YEAR),(WORD)globalTrip.getStartYear());
	eeprom_update_word((WORD *)(offset+MINUTES_ELAPSED),(WORD)globalTrip.getMinutesElapsed());
	eeprom_update_word((WORD *)(offset+DAYS_ELAPSED),(WORD)globalTrip.getDaysElapsed());
	eeprom_update_word((WORD *)(offset+YEARS_ELAPSED),(WORD)globalTrip.getYearsElapsed());
	eeprom_update_word((WORD *)(offset+SPEED_READINGS_L),(WORD)globalTrip.getSpeedPointsLow());
	eeprom_update_word((WORD *)(offset+SPEED_READINGS_H),(WORD)globalTrip.getSpeedPointsHigh());
	eeprom_update_word((WORD *)(offset+HR_READINGS_L),(WORD)globalTrip.getHRReadingsLow());
	eeprom_update_word((WORD *)(offset+HR_READINGS_H),(WORD)globalTrip.getHRReadingsHigh());
	eeprom_update_float((float*)(offset+WHEEL_SIZE),(float)globalTrip.getWheelSize());
	#ifdef TESTING
		char tempString[5],tempString2[5],tempString3[5];
		WORD tempNum=eeprom_read_word((WORD*)(offset+START_DAYS));
		utoa(tempNum,tempString,10);
		PrintBone2("Saved ");
		PrintBone2(tempString);
		PrintBone2("days...");
	#endif
}
/*************************************************************************************************************/
void StartNewTripEEPROM(){
	BYTE numberOfTrips=eeprom_read_byte(&eeNumberOfTrips);				//See how many trips there are currently stored in EEPROM. little endian
	WORD theOffset=INITIAL_OFFSET+((numberOfTrips-1)*BLOCK_SIZE);		//This is the offset, used to get the last wheel size.
	BYTE daysInMonths[12]={31,28,31,30,31,30,31,31,30,31,30,31};		//how many days are in each month
	WORD totalDays=0, tempMonths=0;
	
	//Calculate how many days have been passed, only setting days, not months.	
	totalDays=currentTime.getDays();									//get the total amount of days this month
	tempMonths=currentTime.getMonths();									//get how many months have gone by
	for (int i=0; i<tempMonths-1;i++){									//add all days of the months before the current, if we are in january, doesn't count any.
		totalDays+=daysInMonths[i];
	}
	
	//Get the last wheel size and then set the trip the correct way. Show the trip is now in progress
	double tempWheelSize; 																			
	if (numberOfTrips > 1){tempWheelSize=(double)eeprom_read_float((float*)(theOffset+WHEEL_SIZE));}
	else {tempWheelSize=0;}							//Get the last trips wheel size. Probably haven't changed bikes.
	globalTrip.setTripWTime((double)tempWheelSize, totalDays, currentTime.getYears());				//(re)set the trip, see "trip.h" for function
	eeprom_update_byte(&eeTripFinished,UNFINISHED);													//Signify that the trip is indeed unfinished.
	
	//Update number of global trips in EEPROM. Should already be there.
	numberOfGAVRtrips=numberOfTrips;
	//Good to go, continue on.	
}
/*************************************************************************************************************/
void SaveTripSHUTDOWN(){
	EndTripEEPROM();
	eeprom_update_byte(&eeTripFinished,UNFINISHED);
}
/*************************************************************************************************************/
void LoadLastTripEEPROM(){			//Called on startup.
	BYTE numberOfTrips=eeprom_read_byte(&eeNumberOfTrips);				//little endian
	if (numberOfTrips>1){
		//Update Global number of trips
		numberOfGAVRtrips=numberOfTrips;
		//Get Offset
		WORD offset=INITIAL_OFFSET+((numberOfTrips-1)*BLOCK_SIZE);
		//Load the trip data from EEPROM based on the numberOfTrips. If numberOfTrips=3, need to load the third. AVESPEED for that trip is located at 2+(24*2)
		WORD startDays,startYear,minutesElapsed,daysElapsed,yearsElapsed,speedReadings_L,speedReadings_H,hrReadings_L,hrReadings_H;
		float aveSpeed=eeprom_read_float((float*)(offset+AVESPEED));
		float aveHR=eeprom_read_float((float*)(offset+AVEHR));
		float distance=eeprom_read_float((float*)(offset+DISTANCE));
		startDays=eeprom_read_word((WORD*)(offset+START_DAYS));
		startYear=eeprom_read_word((WORD*)(offset+START_YEAR));
		minutesElapsed=eeprom_read_word((WORD*)(offset+MINUTES_ELAPSED));
		daysElapsed=eeprom_read_word((WORD*)(offset+DAYS_ELAPSED));
		yearsElapsed=eeprom_read_word((WORD*)(offset+YEARS_ELAPSED));
		speedReadings_L=eeprom_read_word((WORD*)(offset+SPEED_READINGS_L));
		speedReadings_H=eeprom_read_word((WORD*)(offset+SPEED_READINGS_H));	//2^16-1=65535, so 65535*this number + SPEED_READINGS_LOW
		hrReadings_L=eeprom_read_word((WORD*)(offset+HR_READINGS_L));
		hrReadings_H=eeprom_read_word((WORD*)(offset+HR_READINGS_H));
		float wheelSize=eeprom_read_float((float*)(offset+WHEEL_SIZE));
		//Set the current trip with these parameters now.
		globalTrip.setOdometer((double)aveSpeed,
							(double)distance,
							(double)wheelSize,
							speedReadings_L,
							speedReadings_H,
							minutesElapsed,
							daysElapsed,
							yearsElapsed,
							startDays,	
							startYear
							);
		globalTrip.setHeartMonitor((double)aveHR, hrReadings_L, hrReadings_H);	
	} else {
		StartNewTripEEPROM();	//if there was a shutdown on the first trip, put in the default stuff. Mainly for testing
	}//end if-else
}//end function
/*************************************************************************************************************/
BOOL StartupTripEEPROM(){
	//This is called when the AVR is rebooted/booted. It looks to see if the trip was finished, if it was then it starts a new trip. If not, it loads the old data
	//from the previous trip and then sets the global trip to those statistics so it can resume.
	BOOL tripFinished=eeprom_read_byte(&eeTripFinished);
	if (tripFinished){
		StartNewTripEEPROM();
		return fTrue;
	} else {
		LoadLastTripEEPROM();
		return fFalse;
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
		eeprom_update_byte(&eeSecond,tempSec);
		eeprom_update_byte(&eeMinute,tempMin);
		eeprom_update_byte(&eeHour,tempHour);
	}
	if (sDate){
		BYTE tempDay,tempMonth;
		WORD tempYear;
		tempYear = currentTime.getYears();
		tempMonth = currentTime.getMonths();
		tempDay = currentTime.getDays();
		eeprom_update_word(&eeYear,tempYear);
		eeprom_update_byte(&eeMonth,tempMonth);
		eeprom_update_byte(&eeDay,tempDay);
	}
	sei();
}

/*************************************************************************************************************/
