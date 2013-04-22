/*******************************************************************************\
| odometer.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 2/26/2013
| Last Revised: 4/7/2013
| Copyright of Todd Sukolsky and Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description: This class is defined as an odometer. Odometer functions include
|	current speed, average speed, time and distance travelled. The "Trip" class
|   inherits this class.
|--------------------------------------------------------------------------------
| Revisions:  2/26: Initial build. Idea is to have Trips inherit this is some way
|			  2/28: Changed a little of the functionality. Added initial time/date
|					feature as well as time elapsed. Should be good, just needs
|					optimizing now.
|			  4/7:  Changed some declarations and added some get functions for resetting trip
|					datas and such.
|================================================================================
| *NOTES:
\*******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include "stdtypes.h"

#define DEFAULT_WHEEL_SIZE .0013882576		//28" wheel size in miles
#define SECONDS_IN_HOUR 3600
#define TIMER1_CLOCK_sec .000032			//Time between ticks in the speed timer...(.032ms)

#define __calculateSpeedWeight() speedWeight=(speedPoints_L-1)/speedPoints_L 

void PrintBone(char string[]);
void PutUartChBone2(char ch);
void PrintBone2(char string[]);

using namespace std;

class odometer{
	public:
		odometer();
		unsigned int getSpeedPointsLow();
		unsigned int getSpeedPointsHigh();
		unsigned int getStartDays();
		unsigned int getStartYear();
		unsigned int getSecondsElapsed();
		unsigned int getMinutesElapsed();
		unsigned int getDaysElapsed();
		unsigned int getYearsElapsed();
		double getCurrentSpeed();
		double getAverageSpeed();
		double getDistance();
		double getWheelSize();
		void addSpeedDataPoint(WORD newDataPoint);
		void addTripSecond();
		void resetSpeedPoints();
		void setStartDate(unsigned int sDays, unsigned int sMonths, unsigned int sYear);
		void setWheelSize(double wheelSize);
		void setOdometer(double swapAveSpeed, double swapDistance,
						double swapWheelSize, unsigned int swapSpeedPoints_L,
						unsigned int swapSpeedPoints_H,unsigned int swapMinutesElapsed,
						unsigned int swapDaysElapsed, unsigned int swapYearsElapsed,
						unsigned int swapSDays, unsigned int swapSYear
						);
				
	protected:
		void setNewOdometerWOtime(double wheelSize=DEFAULT_WHEEL_SIZE);											//New odometer without valid time
		void setNewOdometerWtime(double wheelSize=DEFAULT_WHEEL_SIZE, unsigned int sDate=0, unsigned int sTime=0);	//New odometer with valid time
		void resetOdometer();		//used by trip.h
		//Get functions for normal odometer stuff

	private:
		double aveSpeed, distance, currentSpeed,wheelSize,speedWeight;
		unsigned int sDays, sYear;
		unsigned int tripSeconds,tripMinutes, tripDays, tripYears;
		WORD dataPoints[10];
		unsigned int speedPoints_L, speedPoints_H;	//number of speed points
		BOOL firstRun, noSpeed;
		void updateSpeeds();

};

//For new odometer, initialize everything
odometer::odometer(){
	wheelSize=DEFAULT_WHEEL_SIZE;
	resetOdometer();
}

//How we reset odometer. Everyting is reset except WHEEL SIZE, that's done by the calling function.
void odometer::resetOdometer(){
	firstRun=fTrue;
	noSpeed=fFalse;
	aveSpeed=0;
	distance=0;
	currentSpeed=0;
	speedWeight=0;
	speedPoints_L=0;
	speedPoints_H=0;
	sDays=0;
	sYear=2013;
	tripSeconds=0;
	tripMinutes=0;
	tripDays=0;
	tripYears=0;
	for (int i=0; i<10; i++){
		dataPoints[i]=0;
	}
}


//New odometer with wheel size.
void odometer::setNewOdometerWOtime(double wheelSize){
	this->wheelSize=wheelSize;
	resetOdometer();
}

//New Odometer with wheel size and accurate/valid date and time.
void odometer::setNewOdometerWtime(double wheelSize, unsigned int sDays, unsigned int sYear){
	resetOdometer();
	if (wheelSize<1){this->wheelSize=DEFAULT_WHEEL_SIZE;}
	else {this->wheelSize=wheelSize;}
	this->sDays=sDays;
	this->sYear=sYear;
}

//The date was set just after startup/new trip creation.
void odometer::setStartDate(unsigned int sDays, unsigned int sMonths, unsigned int sYear){
	this->sYear=sYear;
	unsigned int tempDays=sDays;
	BYTE daysInMonths[12]={31,28,31,30,31,30,31,31,30,31,30,31};		//how many days are in each month
	for (int i=0; i<sMonths-1; i++){
		tempDays+=daysInMonths[i];										//add number of days in the months prior
	}
	this->sDays=tempDays;
}

//Restart of the module, need to set everything.
void odometer::setOdometer(double swapAveSpeed, double swapDistance, 
						   double swapWheelSize, unsigned int swapSpeedPoints_L, 
						   unsigned int swapSpeedPoints_H,unsigned int swapMinutesElapsed, 
						   unsigned int swapDaysElapsed, unsigned int swapYearsElapsed,
						   unsigned int swapSDays, unsigned int swapSYear)
{
	aveSpeed=swapAveSpeed;
	distance=swapDistance;
	currentSpeed=0;
	wheelSize=swapWheelSize;
	speedPoints_L=swapSpeedPoints_L;
	speedPoints_H=swapSpeedPoints_H;
	tripMinutes=swapMinutesElapsed;
	tripDays=swapDaysElapsed;
	tripYears=swapYearsElapsed;
	sDays=swapSDays;
	sYear=swapSYear;
	firstRun=fTrue;
}

void odometer::addTripSecond(){
	tripSeconds++;
	if (tripSeconds/60 >=1){
		WORD howManyMinutes=tripSeconds/60;
		tripMinutes+=howManyMinutes;
		//If the number of minutes in the day is a day, add a day. If the number of days is 365, add a year
		if (tripMinutes/1440 >= 1){
			tripDays++;
			if (tripDays/365 >= 1){
				tripYears++;
			}
		}
	}
	//Turn int's into correct values
	tripSeconds%=60;
	tripMinutes%=1440;
	tripDays%=365;	
}
//New speed data point
void odometer::addSpeedDataPoint(WORD newDataPoint){
	//If this is first point or new wheelsize, or whatever, need to initialize all data points. 
	if (firstRun || noSpeed){
		for (int j=0; j<10;j++){
			dataPoints[j]=newDataPoint;
		}
		firstRun=fFalse;				//reset flags
		noSpeed=fFalse;
	//Normal data point. shift back and add new one.
	} else {
		//Shift data back one
		for (int i=0; i<9; i++){
			dataPoints[i]=dataPoints[i+1];	//shift down by one
		}
		dataPoints[9]=newDataPoint;		//add new data point
	}
	
	//Add to amount of speed points.
	//if (++speedPoints_L >= 65534){speedPoints_H++; speedPoints_L=0;}				//increment speed points
	speedPoints_L++;
	//Update distance traveled and time elapsed while riding.
	distance+=wheelSize;			
	
	//With new point we need to update all the statistics.
	updateSpeeds();
}

//The speed went to zero. Need to set flag for startup condition on new speed point, set currentSpeed to 0 so that if we are updating screen
//it won't display old speed, but 0. Reset all speed points.
void odometer::resetSpeedPoints(){
	noSpeed=fTrue;
	currentSpeed=0;
	for (volatile int i=0; i<10; i++){
		dataPoints[i]=0;
	}
}

//Updating wheel size. Don't reset anything, but initialize first run to eliminate old speeds. 
void odometer::setWheelSize(double wheelSize){
	if (wheelSize < 0){
		wheelSize=DEFAULT_WHEEL_SIZE;
	}
	firstRun=fTrue;
	this->wheelSize=wheelSize;
}

//Just got another data point, update all the statistics
void odometer::updateSpeeds(){
	//Update current speed
	double sum=0.0;
	for (volatile int i=0; i<10; i++){
		sum += dataPoints[i];	
	}
	//Everyting is good up until this calculation.
	currentSpeed=10.0*SECONDS_IN_HOUR*wheelSize/(sum*TIMER1_CLOCK_sec);		//calculate and update currentSpeed.
	//Update average speed
	//__calculateSpeedWeight();
	aveSpeed=(aveSpeed*(speedPoints_L-1)+currentSpeed)/speedPoints_L;
}

//Get the current speed
double odometer::getCurrentSpeed(){
	return currentSpeed;
}

//Get the average speed
double odometer::getAverageSpeed(){
	return aveSpeed;
}

//Get the distance travelled thus far in miles 
double odometer::getDistance(){
	return distance;
}

double odometer::getWheelSize(){
	return wheelSize;
}
	
unsigned int odometer::getSpeedPointsLow(){
	return speedPoints_L;
}

unsigned int odometer::getSpeedPointsHigh(){
	return speedPoints_H;
}

unsigned int odometer::getStartDays(){
	return sDays;
}

unsigned int odometer::getStartYear(){
	return sYear;
}

unsigned int odometer::getSecondsElapsed(){
	return tripSeconds;
}

unsigned int odometer::getMinutesElapsed(){
	return tripMinutes;
}

unsigned int odometer::getDaysElapsed(){
	return tripDays;
}

unsigned int odometer::getYearsElapsed(){
	return tripYears;
}








