#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

using namespace std;

int main(){

	unsigned int daysInMonth[12]={31,28,31,30,31,30,31,31,30,31,30,31};
	int trials=5;
	unsigned int tripDays[5]={232,58,59,60,21};
	unsigned int tripMonth=1;
	for (int i=0; i<trials;i++){
		cout << "Testing " << tripDays[i] << " days.\n";
		tripMonth=1;
		while (tripDays[i]>daysInMonth[tripMonth-1]){
			tripDays[i]-=daysInMonth[tripMonth-1];
			tripMonth++;
			cout << "\tTrips days now=" << tripDays[i] << endl;
		}
		if (tripDays[i]>28 && tripMonth==2){
			tripDays[i]-=daysInMonth[tripMonth-1];
			tripMonth++;
			cout << "\tSpecial:";
			cout << "Trip days now=" << tripDays[i] << endl;
		}
		cout << "Month/Day:" << tripMonth <<"/" << tripDays[i] << ".\n" << endl;
	}

	return 0;
}
