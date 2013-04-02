/*******************************************************************************\
| trip.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 2/26/2013
| Last Revised: 2/28/2013
| Copyright of Todd Sukolsky and BU ECE Senior Design teamp Re.Cycle
|================================================================================
| Description: This is a trip class that is used in the GAVR module. Calls things 
|	       from odometer class.
|--------------------------------------------------------------------------------
| Revisions:  2/26: Initial build
|			  2/28: Removed almost everything since it was a bad idea. Made this 
|					guy only inherit the heartMonitor and odometer class. Allows
|					the ability to test with only one of those files.
|================================================================================
| *NOTES: 
\*******************************************************************************/

#include <stdlib.h>
#include "odometer.h"
#include "heartMonitor.h"
#include "stdtypes.h"


using namespace std;

class trip: public odometer, public heartMonitor{
	public:
		trip();	
		void resetTrip();
		void setTripWOTime(double wheelSize);
		void setTripWTime(double wheelSize,unsigned int sDate, unsigned int sTime);
		
};

//Default/no-arg constructor
trip::trip():odometer(), heartMonitor(){
	//Do nothing in this, only important because it's a mix of heartMonitor and odometer.
}

void trip::resetTrip(){
	resetOdometer();
	hardResetHR();
}

void trip::setTripWOTime(double wheelSize){
	setNewOdometerWOtime(wheelSize);
}

void trip::setTripWTime(double wheelSize, unsigned int sDate, unsigned int sTime){
	setNewOdometerWtime(wheelSize, sDate, sTime);
}

