#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cmath>

using namespace std;

int main(){
	int readings[300];
	int peak=0;
	for (int i=0; i< 300; i++){
		readings[i]=sin(i*.085)*450+ 500;
		cout << readings[i] << "  ";
	}
	
	//Search for max and min values in entire array.
	volatile int currentMax=0, currentMax2=0;
	volatile int placeOfAbsMax=0, placeOfAbsMax2=0;
	volatile int place=0;
	
	//Find the absolute maximum in the data
	for (int i=0; i< 150; i++){
		if (readings[i]>currentMax){
			currentMax=readings[i];
			placeOfAbsMax=i;
		}
		if (readings[i+150] > currentMax2){
			currentMax2=readings[i+150];
			placeOfAbsMax2=i+150;
		}	
	}
	
	peak = (currentMax >= currentMax2) ? currentMax: currentMax2;
	placeOfAbsMax= (currentMax >= currentMax2)? placeOfAbsMax : placeOfAbsMax2;
	
	cout << "Found max at " << placeOfAbsMax << " with value " << peak << endl;
	
	//Find local maxima's
	volatile int currentHigh=0, currentHighPlace=0;
	volatile int numberOfBumps=0, downwardSlope=0, upwardSlope=0;
	volatile bool flagGotBeat=false, flagSawUpwardsSlope=false;
	
	//If absolute max was in the first 2/3 ish, start there and go up, if we don't find something then go forward.
	if (placeOfAbsMax < 220){
		place=placeOfAbsMax+10;	//start 10 above current place
		currentHigh=readings[place];
		//Loop throug hthe array looking for another maxima. Compare value to next value. Need to detect (1) an upward slope up to a maximum, then if we recognize a downward slope we can recalculate
		for (int i=place; i < 300; i++){
			//If current reading is higher than current high, make it the new high
			if (readings[i] > currentHigh){
				currentHigh=readings[i];
				currentHighPlace=i;
				downwardSlope=0;
				if (!flagSawUpwardsSlope){upwardSlope++;}
			} else {
				//Next reading is less, add to downward slope, could have maxima
				if (readings[i] => readings[i+1]){downwardSlope++;}
				else if (readings[i] < readings[i+1] && numberOfBumps <= 3) {numberOfBumps++;}
				else if (readings[i] <= readings[i+1] && numberOfBumps > 3) {downwardSlope=0; currentHigh=readings[i+1]; numberOfBumps=0;}
				else;			
			}//end else
			//If we've had a downward slope of 15 then we probably are looking at a local maxima. Bingo. Record place, raise flag, break out of for loop.
			if (downwardSlope > 12 && flagSawUpwardsSlope){flagGotBeat=true; break;}
			if (upwardSlope > 5){flagSawUpwardsSlope=true;}
		} //end for
	
	//If maximum was at the end of the sample OR we didn't find a local maxima in the lower bit of the data
	} else if (placeOfAbsMax > 220 || !flagGotBeat){
		place=placeOfAbsMax-10;	//start from 10 below this place
		downwardSlope=0;
		currentHigh=0;
		numberOfBumps=0;
		currentHigh=readings[place];
		flagSawUpwardsSlope=false;
		//Loop through the array looking for another maxima, have to start from top and work backwards
		for (int i=place; i > 0; i--){
			//If the current readings is greater than last recorded high, make that the current High
			if (readings[i] > currentHigh){
				currentHigh=readings[i];
				downwardSlope=0;
				if (!flagSawUpwardsSlope){upwardSlope++;}
			//If the current reading is less than the current high, look to see if it is less than the previous array value
			} else {
				//If less than previous array value, increment downwardSlope to signal a possible maxima at currentHigh
				if (readings[i] <= readings[i+1]){downwardSlope++;}
				//If current reading is greater than value after this one and numberOfBumps is four or more, make this current high and reset downward slope to start counting again
				else if (readings[i] => readings[i+1] && numberOfBumps > 3){downwardSlope=0;currentHigh=readings[i+1];numberOfBumps=0;}
				else if (readings[i] > readings[i+1] && numberOfBumps <= 3){numberOfBumps++;}
				else;
			}
			//If there was a downward slope for more than 12 readings, we have a local maxima. Save the place and raise the flag, then break
			if (downwardSlope > 12 && flagSawUpwardsSlope){flagGotBeat=true; break;}
			if (upwardSlope > 5){flagSawUpwardsSlope=true;}
		}
	}//end if placeOfAbsMax > 100
	
	double currentHR=0;
	if (flagGotBeat){
		volatile double sampleTimeDifference=abs(placeOfAbsMax-currentHighPlace)*.004;
		currentHR=60000.0/sampleTimeDifference;
	} else {
		currentHR=0;
	}	
	
	cout << "Absolute max = " << readings[placeOfAbsMax] << " , with other maxima of " << readings[currentHighPlace] << " at " << currentHighPlace << endl;
	return 0;
}