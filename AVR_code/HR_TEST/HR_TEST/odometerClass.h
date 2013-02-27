/*******************************************************************************\
| odometerClass.h
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
		void setWheelSize(float wheelSize);
		void addDataPoint(unsigned int newDataPoint);
		float getCurrentSpeed();
		float getAverageSpeed();
		float getDistance();		

	private:
		bool firstRun;
		float aveSpeed, distance, currentSpeed,wheelSize,speedWeight;
		unsigned int dataPoints[10];
		unsigned int speedPoints;	//number of speed points
		void updateStats();
}

odometer::odometer(){
	firstRun=true;
	aveSpeed=0;
	distane=0;
	currentSpeed=0;
	speedWeight=0;
	speedPoints=0;
	wheelSize=DEFAULT_WHEEL_SIZE;
	for (int i=0; i<10; i++){
		dataPoints[i]=0;
	}
}

odometer::odometer(float wheelSize){
	firstRun=true;
	this->wheelSize=wheelSize;
	aveSpeed=0;
	distane=0;
	currentSpeed=0;
	speedWeight=0;
	speedPoints=0;
	for (int i=0; i<10; i++){
		dataPoints[i]=0;
	}
}

odometer::addDataPoint(unsigned int newDataPoint){
	if (firstRun){
		for (int j=0; j<10;j++){
			dataPoints[j]=newDataPoint;
		}
		firstRun=false;
	} else {
		for (int i=0; i<9; i++){
			dataPoints[i]=dataPoints[i+1];	//shift down by one
		}
		dataPoints[9]=newDataPoint;
	}
	//With new point we need to update al the statistics.
	updateStats();
}

void odometer::setWheelSize(float wheelSize){
	speedPoints=0;
	bool firstRun=true;
	this->wheelSize=wheelSize;
}

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
	averageSpeed=averageSpeed*speedWeight + currentSpeed/speedPoints;
}

float odometer::getCurrentSpeed(){
	return currentSpeed;
}

float odometer::getAverageSpeed(){
	return averageSpeed;
}

float odometer::getDistance(){
	return distance;
}

		






















