//Method numbern two, just looking for peaks after minimums
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
	//Now find the overall maximums. Find the maximum, then find the minimum.
	for (int i=0; i<300; i++){
		if (readings[i]<readings[currentMin]){
			currentMin=i;
		}
		if (readings[i]>readings[currentMax]){
			currentMax=i;
		}
	}
	cout << "Overall max at i=" << currentMax << " with value of " << readings[currentMax] << endl;
	cout << "Overall min at i=" << currentMin << " with value of " << readings[currentMin] << endl;

















	return 0;
}
