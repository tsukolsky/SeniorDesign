/*******************************************************************************\
| odometer.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 2/26/2013
| Last Revised: 2/26/2013
| Copyright of Todd Sukolsky and BU ECE Senior Design teamp Re.Cycle
|================================================================================
| Description: This class is used to keep track of current speed, average speed, 
|	       average heart rate, distance travelled, etc.
|--------------------------------------------------------------------------------
| Revisions:  **Make this something that is inherited by Trip Class.**
|================================================================================
| *NOTES:
\*******************************************************************************/

#include <stdlib.h>

#define DEFAULT_WHEEL_SIZE .0013882576		//28" wheel size
#define SECONDS_IN_HOUR 3600
#define TIMER1_CLOCK_sec .000032

#define __calculateSpeedWeight() speedWeight=speedPoints-1/speedPoints;

using namespace std;

class odometer{
	public:
		odometer();
		odometer(float wheelSize);
		odometer(float swapAveSpeed, float swapDistance, float swapCurrentSpeed, float swapWheelSize, unsigned int swapSpeedPoints);
		void setOdometer(float wheelSize);		
		void setWheelSize(float wheelSize);
		void addDataPoint(unsigned int newDataPoint);
		unsigned int getSpeedPoints();
		float getCurrentSpeed();
		float getAverageSpeed();
		float getDistance();
		float getWheelSize();	
	
	private:
		bool firstRun;
		float aveSpeed, distance, currentSpeed,wheelSize,speedWeight;
		unsigned int dataPoints[10];
		unsigned int speedPoints;	//number of speed points
		void updateStats();
};

//For new odometer, initialize everything
odometer::odometer(){
	firstRun=true;
	aveSpeed=0;
	distance=0;
	currentSpeed=0;
	speedWeight=0;
	speedPoints=0;
	wheelSize=DEFAULT_WHEEL_SIZE;
	for (int i=0; i<10; i++){
		dataPoints[i]=0;
	}
}

//New odometer with wheel size.
odometer::odometer(float wheelSize){
	firstRun=true;
	this->wheelSize=wheelSize;
	aveSpeed=0;
	distance=0;
	currentSpeed=0;
	speedWeight=0;
	speedPoints=0;
	for (int i=0; i<10; i++){
		dataPoints[i]=0;
	}
}

odometer::odometer(float swapAveSpeed, float swapDistance, float swapCurrentSpeed, float swapWheelSize, unsigned int swapSpeedPoints){
	aveSpeed=swapAveSpeed;
	distance=swapDistance;
	currentSpeed=swapCurrentSpeed;
	wheelSize=swapWheelSize;
	speedPoints=swapSpeedPoints;
}

//New speed data point
void odometer::addDataPoint(unsigned int newDataPoint){
	//If this is first point or new wheelsize, or whatever, need to initialize all data points.
	if (firstRun){
		for (int j=0; j<10;j++){
			dataPoints[j]=newDataPoint;
		}
		firstRun=false;				//reset flag
	} else {
		//Shift data back one
		for (int i=0; i<9; i++){
			dataPoints[i]=dataPoints[i+1];	//shift down by one
		}
		dataPoints[9]=newDataPoint;		//add new data point
	}
	speedPoints++;					//increment speed points

	//With new point we need to update all the statistics.
	updateStats();
}

//Updating wheel size. Don't reset anything, but initialize first run to eliminate old speeds. 
void odometer::setWheelSize(float wheelSize){
	bool firstRun=true;
	this->wheelSize=wheelSize;
}

//Just got another data point, update all the statistics
void odometer::updateStats(){
	//Update distance
	distance += wheelSize;
	
	//Update current speed
	unsigned int sum;
	for (int i=0; i<10; i++){
		sum += dataPoints[i]/10;
	}
	currentSpeed=SECONDS_IN_HOUR*wheelSize/(sum*TIMER1_CLOCK_sec);
	
	//Update average speed
	__calculateSpeedWeight();
	aveSpeed=aveSpeed*speedWeight + currentSpeed/speedPoints;
}

//Get the current speed
float odometer::getCurrentSpeed(){
	return currentSpeed;
}

//Get the average speed
float odometer::getAverageSpeed(){
	return aveSpeed;
}

//Get the distance travelled thus far in miles 
float odometer::getDistance(){
	return distance;
}

float odometer::getWheelSize(){
	return wheelSize;
}
		
unsigned int odometer::getSpeedPoints(){
	return speedPoints;
}

//Setting an already existing odometer, same thing as initial startup with wheelSize, just don't want to duplicate. THis might be obsolete/pointless because you can't call a specific polymorphed function in a child...maybe calling a new odometer deletes the old ones. Need to test.
void odometer::setOdometer(float wheelSize){
	firstRun=true;
	aveSpeed=0;
	distance=0;
	currentSpeed=0;
	speedWeight=0;
	speedPoints=0;
	this->wheelSize=wheelSize;
	for (int i=0; i<10; i++){
		dataPoints[i]=0;
	}
}
	



















