//Method numbern two, just looking for peaks after minimums
//Got method working roughly. Need to add error/"bump" functionality.
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

int main(){
	float readings[300];
	int peak=0;
	for (int i=0; i< 300; i++){
		float temp=sin(i*.085);
		if (temp<0){
			temp *= -1;
		}
		readings[i]=temp*350+ 400;
		cout << readings[i] << "  ";
	}
	cout << endl;
	unsigned int currentMax=0, currentMin=0;
	float sum=0;
	//Now find the overall maximums. Find the maximum, then find the minimum.
	for (int i=0; i<300; i++){
		sum+=readings[i];
		if (readings[i]<readings[currentMin]){
			currentMin=i;
		}
		if (readings[i]>readings[currentMax]){
			currentMax=i;
		}
	}
	cout << "Overall max at i=" << currentMax << " with value of " << readings[currentMax] << endl;
	cout << "Overall min at i=" << currentMin << " with value of " << readings[currentMin] << endl;

	//Find max's by taking the average.
	float average=sum/300.0;
	cout << "Average is " << average << endl;

	//Go from the max. If it's more than 200 in, go backwards. If we sample every 6ms, heart beat @200BMP=.3 seconds==50 sample apart at .006s sampling interval.
	//A 40BPM heart rate takes 1.33 seconds = 221
	//If the readings is > half the difference between (max -average)
	unsigned int placement=currentMax, flagFound=0, localMax=0,goingDown=0;
	if (placement > 200){
		placement-=10;
		localMax=placement;
		for (placement > 0; placement--;){
		cout << "Placement=" << placement << endl;
			if (readings[placement] >= (average + (readings[currentMax]-average)/2)){
				cout << "Looking at " << readings[placement] << ", location " << placement << endl;
				//should be near a max, look for the max, if we see three decrementing numbers, assume that was th emaximum
				if (readings[placement] >= readings[localMax]){
					localMax=placement;
				} else if ((readings[placement] < readings[placement+1]) && (readings[placement] < readings[localMax])){
					goingDown++;
					cout << "Saw downward slope." << endl;
					if (goingDown>2){flagFound=1;break;}
				} else{
					cout << "Saw upward slope, reset..." << endl;
					goingDown=0;
				}
			}//end if above average
			else;	//do nothing, keep looping until you find it.
		}//end for.
		if (flagFound){
			cout << "Found a local maximum at location " << localMax << " with value " << readings[localMax] << endl;
			cout << "Difference in location = " << abs(localMax-currentMax) << endl;
		} else {
			cout << "Could not find a local maximum." << endl;
		}//end if FlagFound
	} else {
		placement+=10;
		localMax=placement;
		for (placement; placement < 300; placement++){
		cout << "Placement=" << placement << endl;
			if (readings[placement] >= (average + (readings[currentMax]-average)/2)){
				cout << "Looking at " << readings[placement] << ", location " << placement << endl;
				//should be near a max, look for the max, if we see three decrementing numbers, assume that was th emaximum
				if (readings[placement] >= readings[localMax]){
					localMax=placement;
				} else if ((readings[placement] < readings[placement-1]) && (readings[placement] < readings[localMax])){
					goingDown++;
					cout << "Saw downward slope." << endl;
					if (goingDown>2){flagFound=1;break;}
				} else{
					cout << "Saw upward slope, reset..." << endl;
					goingDown=0;
				}
			}//end if above average
			else;	//do nothing, keep looping until you find it.
		}//end for.
		if (flagFound){
			cout << "Found a local maximum at location " << localMax << " with value " << readings[localMax] << endl;
			cout << "Difference in location = " << abs(localMax-currentMax) << endl;
		} else {
			cout << "Could not find a local maximum." << endl;
		}//end if FlagFound
	}//end if-else placement<200
	
	return 0;
}
