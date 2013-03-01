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
#include "stdtypes.h"

#define prtLED PORTC
#define ddrLED DDRC
#define bnLED  5
#define bnSPEEDLED  4

#define __calculateHRWeight() hrWeight=(numReadings-1)/numReadings;

using namespace std;

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
		BOOL firstBeat, secondBeat, pulse;
		WORD rate[10];
		void addNewBeat();
		
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
	firstBeat=fTrue;
	secondBeat=fTrue;
	pulse=fFalse;
	timeBetweenPoints=0;
	lastInterval=0;
	for (int i=0; i< 10; i++){
		rate[i]=0;
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
	//Update timeBetweenPoints
	timeBetweenPoints+=timeSinceLast;
	
	//Adjust Peak and Trough Accordingly
	if (adcReading < pulseThresh && timeBetweenPoints > (lastInterval/5)*3){		//adcReadings less than pulseThresh, time inbetween is more than last interval * 3/5
		if (adcReading < trough){
			trough = adcReading;
		}		

		if (adcReading > pulseThresh && adcReading > peak){
			peak = adcReading;
		}			
	}
	
	//If time since last read is more than 250, see if adcReading is above pulseThresh and time is good.
	if (timeBetweenPoints>250){
		if ((adcReading > pulseThresh) && !pulse && (timeBetweenPoints>((lastInterval/5)*3)) && !firstBeat){	//send pulse high
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
		//peak=pulseThresh;
		//trough=pulseThresh;
	}
		
	//Wow, not getting a pulse, reset things
	if (timeBetweenPoints>=20000){
		//Print0("-TIMEOUT-");
		resetMonitor();
	}
}		

//There was a new beat, add it.
void heartMonitor::addNewBeat(){
	//Variables needed to calculate new HR
	volatile WORD runningTotal=0;

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
		for (int i=0; i < 10; i++){
			rate[i]=lastInterval;
			runningTotal+=lastInterval;
		}
	} else {
		for (int i=0; i< 9; i++){
			rate[i]=rate[i+1];
			runningTotal+=rate[i];
		}
		rate[9]=lastInterval;
		runningTotal+=lastInterval;
	}
	
	//Divide by 10 for average over these data points
	runningTotal/=10;			//time it took all of them in milliseconds
	currentHR=60000.0/runningTotal;		//60 seconds in minute, 1000ms in second. Gives BPM
	
	//Calculate weighting factor then make new HR.
	__calculateHRWeight();
	aveHR=aveHR*hrWeight + currentHR/numReadings;
}
