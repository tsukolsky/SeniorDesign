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
|================================================================================
| *NOTES: 
\*******************************************************************************/

#include <stdlib.h>
#include "stdtypes.h"

using namespace std;

class heartMonitor{
	public:
		heartMonitor();
		heartMonitor(float aveHR, unsigned int numReadings);
		void setHeartMonitor(float aveHR, unsigned int numReadings);
		void setAveHR(float aveHR);
		void setNumReadings(unsigned int numReadings);
		float getAveHR();
		unsigned int getNumReadings();
		
	private:
		float aveHR, currentHR;
		unsigned int numReadings;
		
};

heartMonitor::heartMonitor(){
	aveHR=0;
	currentHR=0;
	numReadings=0;
}

void heartMonitor::setHeartMonitor(float aveHR, unsigned int numReadings){
	this->aveHR=aveHR;
	this->numReadings=numReadings;
}

void heartMonitor::setAveHR(float aveHR){
	this->aveHR=aveHR;
}

void heartMonitor::setNumReadings(unsigned int numReadings){
	this->numReadings=numReadings;
}

float heartMonitor::getAveHR(){
	return aveHR;
}

unsigned int heartMonitor::getNumReadings(){
	return numReadings;
}





