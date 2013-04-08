#include <iostream>
#include <cstdlib>
#include <cmath>
#include <string.h>

using namespace std;

int main(){
	cout << "Enter time and date string:";
	char recString[40];
	cin >> recString;
	int strLoc=strlen(recString);
	cout << "strLoc=" << strLoc << endl;
	int counter=1;
	int tempNum[3]={0,0,0}, tempNum1[3]={0,0,0},dmy=0, hms=0, placement=0;
	//Look at string for all locations up to \0, \0 is needed to know when last digits are done
	char tempStringNum[5];
	for (int i=0; i<5; i++){tempStringNum[i]=(char)NULL;}
	while (recString[counter] != '/' && recString[counter] != '\0'){
		cout << "Evaluating \"" << recString[counter] << "\", with counter = " << counter << endl;
		cout << "tempStringNum= " << tempStringNum << ",and hms= " << hms << endl;
		//temporary string that holds a number, new one for each
		//If the character isn't a colon, we haven't gotten 3 int values, and character isn't eof, add to tempStringNum
		if (recString[counter]!=':' && hms<3){
			tempStringNum[placement++]=recString[counter];
			//If haven't gotten 3 int's and character is colon, store int(stringNum) into tempNum[<current time param>]
		} else if (hms<2 && recString[counter] == ':') {
			tempNum[hms++] = atoi(tempStringNum);
			for (int j=0; j <= placement; j++){tempStringNum[j]=NULL;}	//reset the string
			placement=0;												//reset placement
			//If we haven't found 3 int values and current Location is at end, and end char is null, save as second
		} else;
		counter++;
	}//end while
	//Found a '/', assign tempNum
	if (recString[counter] == '/'){
		tempNum[hms] = atoi(tempStringNum);
	} else {
		//Go to exit state
		return 1;
	}
	
	cout << "\n\nGetting date now\n\n";
	for (int j=0; j <= placement; j++){tempStringNum[j]=NULL;}	//reset the string
	placement=0;
	counter++;
	while (recString[counter] != '.' && recString[counter] != '\0' && counter != strLoc){
		cout << "Evaluating \""<< recString[counter] <<"\" with  counter = " << counter << endl;
		if  (recString[counter] != ',' && recString[counter] != '\0' && recString[counter] != '.' && dmy < 3){
			tempStringNum[placement++]=recString[counter];
		} else if (dmy<2 && recString[counter]==','){
			tempNum1[dmy++] = atoi(tempStringNum);
			cout << "tempNum1[" << dmy-1 << "]=" << tempNum1[dmy-1] << endl;
			for (int j=0; j <= placement; j++){tempStringNum[j]=NULL;}
			placement=0;
		} else;
		counter++;
	}
	//Adssign last date
	if (recString[counter] == '.'){
		tempNum1[dmy] = atoi(tempStringNum);
		cout << "tempNum1[" << dmy << "]=" << tempNum1[dmy] << endl;
	} else {
		return 1;
	}
	
	//Make sure the settings are okay before setting the time. If and issue and number of errors is >3, ask user for time
	if (tempNum[0]/24==0 && tempNum[1]/60==0 && tempNum[2]/60==0){
		cout << "Time is " << tempNum[0] << ":" << tempNum[1] << ":" << tempNum[2] << ".\n";
	} else {
		cout << "ACKBADT." << endl;
	}	
	
	if (tempNum1[0]/13==0 && tempNum1[1]/32==0 && tempNum1[2]/2000>=1){
		cout << "Date is " << tempNum1[0] << "," << tempNum1[1] << "," << tempNum1[2] << endl;
	} else {
		cout << "ACKBADD." << endl;
	}
	return 0;
}