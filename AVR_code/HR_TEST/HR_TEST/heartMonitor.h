/*******************************************************************************\
| heartMonitor.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 2/28/2013
| Last Revised: 2/28/2013
| Copyright of Todd Sukolsky and BU ECE Senior Design teamp Re.Cycle
|================================================================================
| Description: This is a heartMonitor class that is used in the GAVR module. Used
|		to track user heart rate and compile average heart rates. The "trip" class
|		inherits from this class. See "trip.h".
|--------------------------------------------------------------------------------
| Revisions:  2/28: Initial build
|			   3/1: Brought in functionality of HR sampling to the class.
|================================================================================
| *NOTES: HR is on a linear model, not polynomially biased with extra weighting
\*******************************************************************************/

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "stdtypes.h"
//#include "uartSend.h"

#define prtLED PORTC
#define ddrLED DDRC
#define bnLED  5
#define bnSPEEDLED  4

#define MAX_DIPS 3				//maximum number of times the data parser can see a ADC reading below the one before wtihout resetting upwardSlots which deals with flag.
#define MAX_BUMPS 3				//''		''	 "	"		                             "		abve ---------------------------------- downwardSlope
#define UPWARD_SLOPE_MINIMUM 5
#define DOWNWARD_SLOPE_MINUMUM 12
#define SEC_PER_SAMPLE .008

#define __calculateHRWeight() hrWeight=(numReadings-1)/numReadings;

using namespace std;

extern BOOL flagUpdateUserStats;

void Print0(char string[]);


class heartMonitor{
	public:
		heartMonitor();
		void resetMonitor();
		void setHeartMonitor(double aveHR, unsigned int numReadings);
		void setAveHR(double aveHR);
		void setNumReadings(unsigned int numReadings);
		void newHRSample(WORD adcReading, unsigned int timeSinceLast);
		double getAveHR();
		double getCurrentHR();
		unsigned int getNumReadings();
		
	private:
		double aveHR, currentHR, hrWeight;
		unsigned int numReadings;
		unsigned int timeBetweenPoints, lastInterval, pulseThresh, peak, trough;
		BOOL firstRun, secondBeat, pulse;
		volatile WORD rate[6];
		volatile WORD readings[150];	//array for 300 readings
		void addNewBeat();
		void calculateHR();
		
};

heartMonitor::heartMonitor(){
	aveHR=0;
	numReadings=0;
	__calculateHRWeight();
	resetMonitor();
}

//Resets everythin except number of readings and aveHR
void heartMonitor::resetMonitor(){
	currentHR=0;
	pulseThresh=512;
	peak=512;
	trough=512;
	firstRun=fTrue;
	secondBeat=fTrue;
	pulse=fFalse;
	timeBetweenPoints=0;
	lastInterval=0;
	for (int i=0; i< 6; i++){
		rate[i]=0;
	}
	for (int j=0; j<150; j++){
		readings[j]=0;
	}
}

void heartMonitor::setHeartMonitor(double aveHR, unsigned int numReadings){
	this->aveHR=aveHR;
	this->numReadings=numReadings;
	resetMonitor();
}

void heartMonitor::setAveHR(double aveHR){
	this->aveHR=aveHR;
}

void heartMonitor::setNumReadings(unsigned int numReadings){
	this->numReadings=numReadings;
}

double heartMonitor::getCurrentHR(){
	return currentHR;
}

double heartMonitor::getAveHR(){
	return aveHR;
}

unsigned int heartMonitor::getNumReadings(){
	return numReadings;
}

void heartMonitor::newHRSample(WORD adcReading, unsigned int timeSinceLast){
	
	
	/*******************************************METHOD 1**************************************\
	//Update timeBetweenPoints
	timeBetweenPoints+=timeSinceLast;
	
	//Adjust Peak and Trough Accordingly
	if (timeBetweenPoints > (lastInterval/5)*2){		//adcReadings less than pulseThresh, time inbetween is more than last interval * 3/5
		if (adcReading < trough && adcReading < pulseThresh){
			trough = adcReading;
		}		

		if (adcReading > pulseThresh && adcReading > peak){
			peak = adcReading;
		}			
	}
	
	//If time since last read is more than 250, see if adcReading is above pulseThresh and time is good.
	if (timeBetweenPoints > 250){
		if ((adcReading > pulseThresh) && !pulse && !firstBeat){	//send pulse high
			addNewBeat();
		} else if (firstBeat){
			firstBeat=fFalse;
		}		
	}//end if N>250
				
	//No pulse after last interrupt/pulse, send adcReading low again, reset things.
	if (adcReading < pulseThresh && pulse){
		prtLED &= ~(1 << bnLED);
		pulse=fFalse;
		pulseThresh=(peak-trough)/2+trough;
		peak=pulseThresh;
		trough=pulseThresh;
	}
		
	//Wow, not getting a pulse, reset things
	if (timeBetweenPoints>=2000){
		Print0("-TIMEOUT-");
		resetMonitor();
	}
	
	\*************************************END Method 1****************************************************/
	
	/*************************************Method 2********************************************************/

	volatile static WORD currentPlace=0;
	
	if (currentPlace++ < 150){readings[currentPlace]=adcReading;}
	else {currentPlace=0;readings[currentPlace]=adcReading; calculateHR();}
	
}		


//There was a new beat, add it.
void heartMonitor::addNewBeat(){
	
	/*************************************Method 1***********************************************\
	//Print0("-BEAT-");
	//flagUpdateUserStats=fTrue;
	//Variables needed to calculate new HR
	volatile float runningTotal=0;

	//Update number of readings
	numReadings++;
	
	//Logic for new HR. Send pulse high, update the "lastInterval" variable. 
	pulse=fTrue;
	prtLED |= (1 << bnLED);		//turn LED on
	lastInterval=timeBetweenPoints;
	timeBetweenPoints=0;
	//If second beat, everything needs to be initialized up to 10.
	if (secondBeat){
		secondBeat=fFalse;
		for (int i=0; i < 6; i++){
			rate[i]=lastInterval;
			runningTotal+=lastInterval;
		}
	} else {
		for (int i=0; i< 5; i++){
			rate[i]=rate[i+1];
			runningTotal+=rate[i];
		}
		rate[5]=lastInterval;
		runningTotal+=lastInterval;
	}
	
	//Divide by 10 for average over these data points
	runningTotal/=6;			//time it took all of them in milliseconds
	runningTotal*=.002;			//each tick is .002 seconds
	currentHR=60/runningTotal;		//60 seconds in minute, 1000ms in second. Gives BPM
	
	\********************************************END Method 1*************************************************/
	
	
	//Calculate weighting factor then make new HR.
	__calculateHRWeight();
	aveHR=aveHR*hrWeight + currentHR/numReadings;
	
}


void heartMonitor::calculateHR(){
	//Search for max and min values in entire array.
	volatile WORD currentMax=0, currentMax2=0;
	volatile BYTE placeOfAbsMax=0, placeOfAbsMax2=0;
	volatile int place=0;
	
	//Find the absolute maximum in the data
	for (int i=0; i< 75; i++){
		if (readings[i]>currentMax){
			currentMax=readings[i];
			placeOfAbsMax=i;
		}
		if (readings[i+75] > currentMax2){
			currentMax2=readings[i+75];
			placeOfAbsMax2=i+75;
		}	
	}
	peak= (currentMax >= currentMax2) ? currentMax: currentMax2;
	placeOfAbsMax= (currentMax >= currentMax2)? placeOfAbsMax : placeOfAbsMax2;
	
	
	char absMaxString[10];
	utoa(peak,absMaxString,10);
	absMaxString[9]='\0';
	absMaxString[9]='.';
	Print0("abs max=");
	Print0(absMaxString);
	
	
	//Find local maxima's
	volatile WORD currentHigh=0, currentHighPlace=0;
	volatile BYTE numberOfDips=0, numberOfBumps=0, downwardSlope=0, upwardSlope=0;
	volatile BOOL flagGotBeat=fFalse, flagSawUpwardsSlope=fFalse;
	
	
	//If absolute max was in the first 2/3 ish, start there and go up, if we don't find something then go forward.
	if (placeOfAbsMax < 80){
		place=placeOfAbsMax+10;	//start 10 above current place
		currentHigh=readings[place];
		//Loop throug hthe array looking for another maxima. Compare value to next value. Need to detect (1) an upward slope up to a maximum, then if we recognize a downward slope we can recalculate
		for (int i=place; i < 150; i++){
			//If current reading is higher than current high, make it the new high
			if (readings[i] > currentHigh){
				currentHigh=readings[i];
				currentHighPlace=i;
				downwardSlope=0;
				numberOfBumps=0;
				if (!flagSawUpwardsSlope){upwardSlope++;}
			} else {
				if (!flagSawUpwardsSlope){
					if (numberOfDips++ >= MAX_DIPS){upwardSlope=0; numberOfDips=0;}
				}				
				//Next reading is less, add to downward slope, could have maxima
				if (readings[i] >= readings[i+1]){downwardSlope++;}
				else if (readings[i] < readings[i+1] && numberOfBumps <= MAX_BUMPS) {numberOfBumps++;}
				else if (readings[i] <= readings[i+1] && numberOfBumps > MAX_BUMPS) {downwardSlope=0; currentHigh=readings[i+1]; numberOfBumps=0;}
				else;
			}//end else
			//If we've had a downward slope of 15 then we probably are looking at a local maxima. Bingo. Record place, raise flag, break out of for loop.
			if (downwardSlope > DOWNWARD_SLOPE_MINUMUM && flagSawUpwardsSlope){flagGotBeat=fTrue; break;}
			if (upwardSlope > UPWARD_SLOPE_MINIMUM){flagSawUpwardsSlope=fTrue;}
		} //end for
	
	//If maximum was at the end of the sample OR we didn't find a local maxima in the lower bit of the data
	} else if (placeOfAbsMax >= 80 || !flagGotBeat){
		place=placeOfAbsMax-10;	//start from 10 below this place
		downwardSlope=0;
		currentHigh=0;
		currentHighPlace=0;
		numberOfBumps=0;
		currentHigh=readings[place];
		flagSawUpwardsSlope=fFalse;
		numberOfDips=0;
		//Loop through the array looking for another maxima, have to start from top and work backwards
		for (int i=place; i > 0; i--){
			//If the current readings is greater than last recorded high, make that the current High
			if (readings[i] > currentHigh){
				currentHigh=readings[i];
				currentHighPlace=i;
				downwardSlope=0;
				numberOfBumps=0;
				if (!flagSawUpwardsSlope){upwardSlope++;}
			//If the current reading is less than the current high, look to see if it is less than the previous array value
			} else {
				if (!flagSawUpwardsSlope){
					if (numberOfDips++ >= MAX_DIPS){upwardSlope=0; numberOfDips=0;}
				}
				//If less than previous array value, increment downwardSlope to signal a possible maxima at currentHigh
				if (readings[i] <= readings[i+1]){downwardSlope++;}
				//If current reading is greater than value after this one and numberOfBumps is four or more, make this current high and reset downward slope to start counting again
				else if (readings[i] >= readings[i+1] && numberOfBumps > MAX_BUMPS){downwardSlope=0;currentHigh=readings[i+1];numberOfBumps=0;}
				else if (readings[i] > readings[i+1] && numberOfBumps <= MAX_BUMPS){numberOfBumps++;}
				else;
			}
			//If there was a downward slope for more than 12 readings, we have a local maxima. Save the place and raise the flag, then break
			if (downwardSlope > DOWNWARD_SLOPE_MINUMUM && flagSawUpwardsSlope){flagGotBeat=fTrue; break;}
			if (upwardSlope > UPWARD_SLOPE_MINIMUM){flagSawUpwardsSlope=fTrue;}
		}
	}//end if placeOfAbsMax > 100
	
	if (flagGotBeat){
		volatile double sampleTimeDifference=abs(placeOfAbsMax-currentHighPlace)*SEC_PER_SAMPLE;
		currentHR=1/sampleTimeDifference;
		numReadings++;
	} else {
		currentHR=0;
	}
	flagUpdateUserStats=fTrue;
}