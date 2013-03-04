/*******************************************************************************\
| odometer.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 2/26/2013
| Last Revised: 2/28/2013
| Copyright of Todd Sukolsky and BU ECE Senior Design teamp Re.Cycle
|================================================================================
| Description: This class is defined as an odometer. Odometer functions include
|	current speed, average speed, time and distance travelled. The "Trip" class
|   inherits this class.
|--------------------------------------------------------------------------------
| Revisions:  2/26: Initial build. Idea is to have Trips inherit this is some way
|			  2/28: Changed a little of the functionality. Added initial time/date
|					feature as well as time elapsed. Should be good, just needs
|					optimizing now.
|================================================================================
| *NOTES:
\*******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include "stdtypes.h"

#define DEFAULT_WHEEL_SIZE .0013882576		//28" wheel size
#define SECONDS_IN_HOUR 3600
#define TIMER1_CLOCK_sec .000032

#define __calculateSpeedWeight() speedWeight=speedPoints-1/speedPoints;

using namespace std;

//The RTC being used.
//extern theClock;

class odometer{
	public:
		odometer();
		void setNewOdometerWOtime(double wheelSize=DEFAULT_WHEEL_SIZE);											//New odometer without valid time
		void setNewOdometerWtime(double wheelSize=DEFAULT_WHEEL_SIZE, unsigned int sDate=0, unsigned int sTime=0);	//New odometer with valid time
		void setOdometer(double swapAveSpeed, double swapDistance, double swapCurrentSpeed, double swapWheelSize, unsigned int swapSpeedPoints,unsigned int timeElapsed,unsigned int sDate, unsigned int sTime);	//Had shutdown, set the odometer with all the statistics.	
		/*Functions we need*\
		void setOdometerTime(unsigned int sDate, unsigned int sTime, unsigned int timeElapsed);
		\*******************/
		void setWheelSize(double wheelSize);
		void addSpeedDataPoint(unsigned int newDataPoint);
		void resetSpeedPoints();
		
		//Get functions for normal odometer stuff
		unsigned int getSpeedPoints();
		double getCurrentSpeed();
		double getAverageSpeed();
		double getDistance();
		double getWheelSize();	
		//Stuff for time and date
		unsigned int getTotalTime(unsigned int eTime, unsigned int eDate);
		
	private:
		double aveSpeed, distance, currentSpeed,wheelSize,speedWeight;
		unsigned int sDate,sTime;
		unsigned int timeElapsed;
		unsigned int dataPoints[10];
		unsigned int speedPoints;	//number of speed points
		BOOL firstRun, noSpeed;
		void updateSpeeds();
		void resetOdometer();
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
	speedPoints=0;
	sDate=0;
	sTime=0;
	timeElapsed=0;
	for (volatile int i=0; i<10; i++){
		dataPoints[i]=0;
	}
}


//New odometer with wheel size.
void odometer::setNewOdometerWOtime(double wheelSize){
	this->wheelSize=wheelSize;
	resetOdometer();
}

//New Odometer with wheel size and accurate/valid date and time.
void odometer::setNewOdometerWtime(double wheelSize, unsigned int sDate, unsigned int sTime){
	this->wheelSize=wheelSize;
	this->sDate=sDate;
	this->sTime=sTime;
	resetOdometer();
}

//Restart of the module, need to set everything.
void odometer::setOdometer(double swapAveSpeed, double swapDistance, double swapCurrentSpeed, double swapWheelSize, unsigned int swapSpeedPoints,unsigned int swapTimeElapsed,unsigned int swapSDate, unsigned int swapSTime){
	aveSpeed=swapAveSpeed;
	distance=swapDistance;
	currentSpeed=swapCurrentSpeed;
	wheelSize=swapWheelSize;
	speedPoints=swapSpeedPoints;
	timeElapsed=swapTimeElapsed;
	sDate=swapSDate;
	sTime=swapSTime;
	firstRun=fTrue;
}

//New speed data point
void odometer::addSpeedDataPoint(unsigned int newDataPoint){
	//If this is first point or new wheelsize, or whatever, need to initialize all data points.
	if (firstRun || noSpeed){
		for (volatile int j=0; j<10;j++){
			dataPoints[j]=newDataPoint;
		}
		firstRun=fFalse;				//reset flags
		noSpeed=fFalse;
	//Normal data point. shift back and add new one.
	} else {
		//Shift data back one
		for (volatile int i=0; i<9; i++){
			dataPoints[i]=dataPoints[i+1];	//shift down by one
		}
		dataPoints[9]=newDataPoint;		//add new data point
	}
	speedPoints++;					//increment speed points
	distance+=wheelSize;			//increment distance.

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
	firstRun=fTrue;
	this->wheelSize=wheelSize;
}

//Just got another data point, update all the statistics
void odometer::updateSpeeds(){

	//Update current speed
	unsigned int sum;
	for (volatile int i=0; i<10; i++){
		sum += dataPoints[i];
	}
	currentSpeed=10.0*SECONDS_IN_HOUR*wheelSize/(sum*TIMER1_CLOCK_sec);		//calculate and update currentSpeed.
	
	//Update average speed
	__calculateSpeedWeight();
	aveSpeed=aveSpeed*speedWeight + currentSpeed/speedPoints;
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
	
unsigned int odometer::getSpeedPoints(){
	return speedPoints;
}

unsigned int odometer::getTotalTime(unsigned int eDate, unsigned int eTime){
	//Compute time between sDate/Time and eDate/Time, then add time elapsed.
	return timeElapsed;
}













