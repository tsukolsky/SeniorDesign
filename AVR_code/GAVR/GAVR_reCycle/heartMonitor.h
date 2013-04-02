/*******************************************************************************\
| heartMonitor.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 2/28/2013
| Last Revised: 3/4/2013
| Copyright of Todd Sukolsky and BU ECE Senior Design teamp Re.Cycle
|================================================================================
| Description: This is a heartMonitor class that is used in the GAVR module. Used
|		to track user heart rate and compile average heart rates. The "trip" class
|		inherits from this class. See "trip.h".
|--------------------------------------------------------------------------------
| Revisions:  2/28: Initial build
|			   3/1: Brought in functionality of HR sampling to the class.
|			   3/4: Changed Heart Rate algorithm. Able to find the actual HR
|					every three or four measurements. Currently implementing a 
|					sliding window method to find maximas, however there is noise
|					and the correct value does not always come out. One solution
|					is to implement a differential function and find where a high 
|					slope changes from positive to negative. Also, can find average of three-6
|					points and measure against the average of another 6 points 3 points foward.
|					If there is a jump, bingo. More later...
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

#define UPWARD_SLOPE_MINIMUM 10
#define DOWNWARD_SLOPE_MINUMUM 10
#define SEC_PER_SAMPLE .008

#define __calculateHRWeight() hrWeight=(numReadings-1)/numReadings;

using namespace std;

//extern BOOL flagUpdateUserStats;

void Print0(char string[]);
void PutUart0Ch(char ch);


class heartMonitor{
	public:
		heartMonitor();
		void resetMonitor();
		void setHeartMonitor(double aveHR, unsigned int numReadings);
		void setAveHR(double aveHR);
		void setNumReadings(unsigned int numReadings);
		void calculateHR(unsigned int *samples, int size);
		double getAveHR();
		double getCurrentHR();
		unsigned int getNumReadings();
	
	protected: 
		void hardResetHR();				//used by trip.h
		
	private:
		double aveHR, currentHR, hrWeight;
		unsigned int numReadings;
		void softResetHR();
		
};

heartMonitor::heartMonitor(){
	hardResetHR();
}

//Resets everythin
void heartMonitor::hardResetHR(){
	currentHR=0;
	numReadings=0;
	aveHR=0;
	__calculateHRWeight();
}

//Resets everything except AVEHR and Num readings.
void heartMonitor::softResetHR(){
	currentHR=0;
	__calculateHRWeight();
}

void heartMonitor::setHeartMonitor(double aveHR, unsigned int numReadings){
	this->aveHR=aveHR;
	this->numReadings=numReadings;
	softResetHR();
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

void heartMonitor::calculateHR(unsigned int *samples, int size){
	//Search for max and min values in entire array.
	volatile WORD currentMax=0, currentMax2=0;
	volatile BYTE placeOfAbsMax=0, placeOfAbsMax2=0;
	
	/*Debugging
	for (int i=0; i< size; i+=3){
		char tempString[10];
		utoa(samples[i],tempString,10);
		tempString[9]='\0';
		tempString[8]='-';
		PutUart0Ch('-');
		Print0(tempString);
	}	*/
	
	
	//Find the absolute maximum in the data
	for (int i=0; i< size/2; i++){
		if (samples[i]>currentMax){
			currentMax=samples[i];
			placeOfAbsMax=i;
		}
		if (samples[i+ size/2] > currentMax2){
			currentMax2=samples[i+size/2];
			placeOfAbsMax2=i+size/2;
		}	
	}
	unsigned int peak= (currentMax >= currentMax2) ? currentMax: currentMax2;
	placeOfAbsMax= (currentMax >= currentMax2)? placeOfAbsMax : placeOfAbsMax2;
	
	/*Debugging
	char absMaxString[10];
	utoa(peak,absMaxString,10);
	absMaxString[9]='\0';
	absMaxString[9]='.';
	Print0("abs max=");
	Print0(absMaxString);*/
	
	
	//Find local maxima's
	volatile WORD currentHigh=0;
	volatile int currentHighPlace=0;
	volatile BYTE downwardSlope=0, upwardSlope=0;
	volatile BOOL flagGotBeat=fFalse;
	volatile int place=0;	
	
	//If absolute max was in the first 2/3 ish, start there and go up, if we don't find something then go forward.
	if (placeOfAbsMax < (size/2)){
		place=placeOfAbsMax+15;	//start 5 above current place
		//Loop through the rest of the array 20 things at a time, find the absolute maximum in that 20. If 4/6 to the left are smaller and 4/6 to the right are smaller, call that a peak.
		for (int i=place; i < size; i+=5){
			currentHigh=samples[i];
			for (int j=i; j<(i+30); j++){
				//Find maximum. 
				if (samples[j] >=currentHigh){
					samples[j]=currentHigh;
					currentHighPlace=j;
				}
			}
			for (int k=currentHighPlace-15; k <= currentHighPlace+15; k++){
				if (k<currentHighPlace && samples[currentHighPlace] < samples[k]){upwardSlope++;}
				if (k>=currentHighPlace && samples[currentHighPlace] >= samples[k]){downwardSlope++;}
			}
			if (downwardSlope > DOWNWARD_SLOPE_MINUMUM && upwardSlope > UPWARD_SLOPE_MINIMUM){flagGotBeat=fTrue; break;}
			else {upwardSlope=0; downwardSlope=0; flagGotBeat=fFalse;}	//do nothing, go into next 20 things.
		} //end for
	
	//If maximum was at the end of the sample OR we didn't find a local maxima in the lower bit of the data
	} else if (placeOfAbsMax >= (size/2) && !flagGotBeat){
		place=placeOfAbsMax-15;
		upwardSlope=0;
		downwardSlope=0;
		for (int i=place; i>0; i-= 5){
			currentHigh=samples[i];
			for (int j=i; j > (i-30); j--){
				if (samples[j] >= currentHigh){
					currentHigh=samples[j];
					currentHighPlace=j;
				}
			}
			for (int k=currentHighPlace+15; k > currentHighPlace-15; k--){
				if (k>=currentHighPlace && samples[currentHighPlace] >= samples[k]){upwardSlope++;}
				if (k<currentHighPlace && samples[currentHighPlace] <= samples[k]){downwardSlope++;}
			}
			if (downwardSlope > DOWNWARD_SLOPE_MINUMUM && upwardSlope > UPWARD_SLOPE_MINIMUM){flagGotBeat=fTrue; break;}	
			else {upwardSlope=0; downwardSlope=0; flagGotBeat=fFalse;}		
		}
	}//end if placeOfAbsMax > 100

	
	if (flagGotBeat){
		/*Debugging
		char distanceString[5];
		volatile int hmm=abs(placeOfAbsMax-currentHighPlace);
		utoa(hmm, distanceString,10);
		distanceString[4]='\0';
		Print0("d=");
		Print0(distanceString);*/
		volatile double sampleTimeDifference=abs(placeOfAbsMax-currentHighPlace)*SEC_PER_SAMPLE;
		currentHR=60/sampleTimeDifference;
		numReadings++;
	} else {
		volatile static unsigned int numZeros=0;
		numZeros++;
		if (numZeros>3){currentHR=0;numZeros=0;}
	}
	/*Debugging
	volatile static int counter=0;
	if (counter++>5){
		flagUpdateUserStats=fTrue;
		counter=0;
	}	*/
}