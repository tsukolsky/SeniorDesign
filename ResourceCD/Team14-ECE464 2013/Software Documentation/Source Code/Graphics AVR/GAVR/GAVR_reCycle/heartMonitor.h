/*******************************************************************************\
| heartMonitor.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 2/28/2013
| Last Revised: 4/23/2013
| Copyright of Todd Sukolsky and Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description: This is a heartMonitor class that is used in the GAVR module. Used
|		to track user heart rate and compile average heart rates. The "trip" class
|		inherits from this class. See "trip.h".
|--------------------------------------------------------------------------------
| Revisions:  2/28: Initial build
|			  3/1:	Brought in functionality of HR sampling to the class.
|			  3/4:	Changed Heart Rate algorithm. Able to find the actual HR
|					every three or four measurements. Currently implementing a 
|					sliding window method to find maximas, however there is noise
|					and the correct value does not always come out. One solution
|					is to implement a differential function and find where a high 
|					slope changes from positive to negative. Also, can find average of three-6
|					points and measure against the average of another 6 points 3 points foward.
|					If there is a jump, bingo. More later...
|			  4/7:  Changed some declarations and added some get functions for resetting trip 
|					datas and such.
|			  4/14: Trying new peak detection...
|			  4/23: Implmeneted new peak detection. Looks for downard slope from a point of the absolute maximum
|					Has some dynamic features such as offset to look. If it sees a quick heart beat, then a slow,
|					will keep the previous until it verifies this. Another optoin is to find multiple peaks if possible.
|					Most of the time the peaks come around the same level given such a small sampling time, better recognition
|					of peaks can be done more effectively. The system does work however, and is accurate to within 4 beats
|					when it recognizes a heart beat consistently. In other words, when it gets a heart rate over adn over, it's
|					the right one, however when it gets the wrong heart rate it changes often. Also changed how average speed operates.
|================================================================================
| *NOTES: HR is on a linear model, not polynomially biased with extra weighting
\*******************************************************************************/

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "stdtypes.h"

#define SEC_PER_SAMPLE .007

#define __calculateHRWeight() hrWeight=(numReadings_L-1)/(numReadings_L);

using namespace std;

//extern BOOL flagUpdateUserStats;

void Print0(char string[]);
void PutUart0Ch(char ch);


class heartMonitor{
	public:
		heartMonitor();
		double getAveHR();
		double getCurrentHR();
		unsigned int getHRReadingsLow();
		unsigned int getHRReadingsHigh();
		void setHeartMonitor(double aveHR, unsigned int numReadings_L, unsigned int numReadings_H);
		void calculateHR(WORD *samples, int size);
		
	protected: 
		void hardResetHR();				//used by trip.h
		void resetMonitor();

	private:
		double aveHR, currentHR, hrWeight;
		unsigned int numReadings_L, numReadings_H;
		void softResetHR();
		WORD lastDifference;
		
};

heartMonitor::heartMonitor(){
	hardResetHR();
}

//Resets everythin
void heartMonitor::hardResetHR(){
	currentHR=0;
	lastDifference=0;
	numReadings_L=0;
	numReadings_H=0;
	aveHR=0;
	__calculateHRWeight();
}

//Resets everything except AVEHR and Num readings.
void heartMonitor::softResetHR(){
	currentHR=0;
	lastDifference=0;
	__calculateHRWeight();
}

void heartMonitor::setHeartMonitor(double aveHR, unsigned int numReadings_L, unsigned int numReadings_H){
	this->aveHR=aveHR;
	this->numReadings_L=numReadings_L;
	this->numReadings_H=numReadings_H;
	softResetHR();
}

double heartMonitor::getCurrentHR(){
	return currentHR;
}

double heartMonitor::getAveHR(){
	return aveHR;
}

unsigned int heartMonitor::getHRReadingsLow(){
	return numReadings_L;
}

unsigned int heartMonitor::getHRReadingsHigh(){
	return numReadings_H;
}

void heartMonitor::calculateHR(WORD *samples, int size){
	//Search for max and min values in entire array.
	WORD currentMax=0, currentMax2=0;
	BYTE placeOfAbsMax=0, placeOfAbsMax2=0;
	double sum=0;
	/*Debugging*//*
	for (int i=0; i< size; i+=2){
		char tempString[10];
		utoa(samples[i],tempString,10);
		tempString[9]='\0';
		tempString[8]='-';
		PutUartChBone2('-');
		PrintBone2(tempString);
	}	*/
	
	
	//Find the absolute maximum in the data
	for (int i=0; i< size/2; i++){
		sum+=samples[i];
		sum+=samples[i+(size/2)];
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
	WORD average=sum/size;
	
	/*Debugging*//*
	char absMaxString[10];
	utoa(peak,absMaxString,10);
	absMaxString[9]='\0';
	absMaxString[9]='.';
	PrintBone2("abs max=");
	PrintBone2(absMaxString);
	char aveString[10];
	utoa(average,aveString,10);
	PrintBone2("Ave data=");
	PrintBone2(aveString);*/
	
	unsigned int placement=placeOfAbsMax, localMax=0,goingDown=0;
	BOOL flagFound=fFalse;
	
	//If abve the 200 mark, go downwards.
	if (placement > 200){
		if (currentHR>20 && currentHR<200){
			placement-=(lastDifference*5/8);
		}
		else {
			placement-=30;
		}
		localMax=placement;
		for (placement > 0; placement--;){
			if (samples[placement] >= (average + (samples[currentMax]-average)*(3/4))){
				//should be near a max, look for the max, if we see three decrementing numbers, assume that was th emaximum
				if (samples[placement] >=samples[localMax]){
					localMax=placement;
				} else if ((samples[placement] < samples[placement+1]) && (samples[placement] < samples[localMax])){
					goingDown++;
					if (goingDown>3){flagFound=fTrue;break;}
				} else{
				//	cout << "Saw upward slope, reset..." << endl;
					goingDown=0;
				}
			}//end if above average
			else;	//do nothing, keep looping until you find it.
		}//end for.
		if (flagFound){
			//PrintBone2("Found a local max down.");
		} else {
			//PrintBone2("Didn't find a local max down.");
		}//end if FlagFound
	} 
	//If we didn't find it or the placement is below 200, look forwards.
	if (placeOfAbsMax < 200 || !flagFound){
		placement=placeOfAbsMax;
		if (currentHR>20 && currentHR<200){
			placement+=(lastDifference*5/8);
		}
		else {
			placement+=30;
		}
		localMax=placement;
		for (placement < 300; placement++;){
			if (samples[placement] >= (average + (samples[currentMax]-average)*(3/4))){
				//should be near a max, look for the max, if we see three decrementing numbers, assume that was th emaximum
				if (samples[placement] >= samples[localMax]){
						localMax=placement;
				} else if ((samples[placement] < samples[placement-1]) && (samples[placement] < samples[localMax])){
					goingDown++;
					//cout << "Saw downward slope." << endl;
					if (goingDown>3){flagFound=fTrue;break;}
				} else{
					//cout << "Saw upward slope, reset..." << endl;
					goingDown=0;
				}
			}//end if above average
			else;	//do nothing, keep looping until you find it.
		}//end for.
		if (flagFound){
			//PrintBone2("Found a local max up.");	
		} else {
			lastDifference=0;
			//PrintBone2("Didn't find a max up.");
			currentHR=0;
		}//end if FlagFound
		
	}//end if-else placement<200
	
	//If the absolute max is less than 30 above the samples, there is no heart rate.
	if ((peak-(WORD)average) < 23){
		currentHR=0.0;
	} else {
		if (flagFound){
			double lastHR=currentHR;
			BYTE weighting=1;
			lastDifference=abs(localMax-placeOfAbsMax);
			float sampleTimeDifference=lastDifference*SEC_PER_SAMPLE;
			currentHR=60.0/sampleTimeDifference;
			WORD differenceInHR=abs((WORD)lastHR - (WORD)currentHR);
			if (differenceInHR < 10){
				weighting=2;
			} else if (differenceInHR >=10 && differenceInHR <20){		//if it's a big jump, don't let it go too far.
				weighting=5;
			} else if (differenceInHR >=20 && differenceInHR < 40){
				weighting=8;
			} else {											//If it's crazy off, let it jump
				weighting=1;
			}	
			//Calculate the new HR
			if (currentHR>lastHR){
				currentHR=lastHR+(differenceInHR/weighting);
			} else {
				currentHR=lastHR-(differenceInHR/weighting);
			}//end if lastHR<currentHR else		
			
			//Calculate the new Average HR
			if (numReadings_L<=65534 && currentHR<200 && currentHR>20){
				numReadings_L++;
			}
			if (currentHR<200 && currentHR>20){
				aveHR=(aveHR*(numReadings_L-1)+currentHR)/(numReadings_L);
			} else {currentHR=0.0;}	
						
		}//end if flag found
		else {currentHR=0.0;}//end if-else flagFound
	}//end if-else
	
}
