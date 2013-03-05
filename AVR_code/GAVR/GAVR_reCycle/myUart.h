/*******************************************************************************\
| myUart.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 3/3/2013
| Last Revised: 3/3/2013
| Copyright of Todd Sukolsky
|================================================================================
| Description: This file conatians the UART0 and UART1 basic send and receive routines used 
|			by the Atmel family of microcontrollers
|--------------------------------------------------------------------------------
| Revisions: 3/3: Initial build
|================================================================================
| *NOTES:
\*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stdtypes.h"

//Implementations for interrupt resets and sets
#define __disableLevel1INT() PCMSK0 = 0x00; EIMSK=0x00;		//disable any interrupt that might screw up reception. Keep the clock ticks going though
#define __enableLevel1INT() PCMSK0 |= (1 << PCINT0); EIMSK |= (1 << INT2);


//Declare external flags and clock.
extern BOOL flagUARTWAVR, flagGetUserDate, flagGetUserTime, justStarted;
extern myTime currentTime;

/**************************************************************************************************************/
void PutUartChBone(char ch){
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0=ch;
}
/*************************************************************************************************************/
void PrintBone(char string[]){
	BYTE i=0;
	
	while (string[i]){
		PutUartChBone(string[i++]);
	}
}
/*************************************************************************************************************/

void PutUartChWAVR(char ch){
	while (!(UCSR1A & (1 << UDRE1)));
	UDR1=ch;
}
/*************************************************************************************************************/

void PrintWAVR(char string[]){
	BYTE i=0;
	while (string[i]){
		PutUartChWAVR(string[i++]);
	}
}
/*************************************************************************************************************/

//Only two things should ever be transmitted to this device. The Time or the request for the user to set date.If the
//request for user to set date happens
void ReceiveWAVR(){
	/*****Kill unnecessary Interrupts*****/
	cli();
	__disableLevel1INT();
	sei();
	
	//Declare variables to be used.
	volatile unsigned int state=0;
	char recString[20];
	char recChar;
	int strLoc=0;
	BOOL noCarriage=fTrue;
	
	do{

		/***************************************************************NEW METHOD-STATE MACHINE*********************************************************/
		/* This state machine has 5 states and is used to receive a string from the WAVR.																*/
		/* State 0: Just got interrupt request, then UART character was transmitted. if the character is a '.', kill connection and print error, else	*/
		/*				add to the recString and go to state 1 to get the rest of the transmitted string.												*/
		/* State 1: Receiving the rest of a string. If recChar is not a '.', add to string with error checking. If it is a '.', add to the string and	*/
		/*				go to state 2 to parse the string.																								*/
		/* State 2: Parse the string and compare to known commands. If It's asking for a date, then go get it and then raise a flag to transmit to WAVR */
		/*				and move to state 4, but if it is a time string then we are getting an update with date and time and move to state 3.			*/
		/* State 3: Take the string and parse it for the time then set if valid. If not valid, send ACKBAD and go to state 4, otherwise save the date   */
		/*			and time in EEPROM as well as set the current Time and then go to state 4.															*/
		/* State 4: This is the close connection state. Reset state to 0 and kill the flagUARTWAVR along with any other necessary flags					*/
		/************************************************************************************************************************************************/
		switch (state){
			case 0: {
				strLoc=0;
				recChar=UDR1;
			if (recChar=='.'){PrintWAVR("ACKERROR.");state=4;}
		else {recString[strLoc++]=recChar; state=1;}
		break;
	}
	case 1: {
		while (noCarriage && flagUARTWAVR){
			while (!(UCSR1A & (1 << RXC1)) && flagUARTWAVR);	//wait for this sucker to come in
			recChar=UDR1;
			recString[strLoc++]=recChar;
		if (recChar=='.'){recString[strLoc]='\0'; noCarriage=fFalse; state=2;}
		else {
			recString[strLoc++] = recChar;
		if (strLoc >= 19){strLoc = 0; noCarriage = fFalse; flagUARTWAVR=fFalse; PrintWAVR("ACKERROR."); state=0;}
	}
}//end while noCarriage
break;
			}
		case 2: {
			if (!strcmp(recString,"SYNDONE.")){state=0;}
			else if (!strcmp(recString,"SYNGD.")){flagGetUserDate=fTrue; PrintWAVR("ACKGD.");state=4;}
			else if (!strcmp(recString,"SYNGT.")){flagGetUserTime=fTrue; PrintWAVR("ACKGT."); state=4;}
			else if (!strcmp(recString,"SYNGB.")){flagGetUserDate=fTrue; flagGetUserTime=fTrue; PrintWAVR("ACKGB."); state=4;}
			else if ((recString[4]==':') != (recString[5]==':')){state=3;}
			else {PrintWAVR("ACKERROR.");state=4;}
			break;
			}
		case 3: {
			int tempNum[3]={0,0,0}, hms=0, placement=0;
			//Look at string for all locations up to \0, \0 is needed to know when last digits are done
			char tempStringNum[3];
			for (int i=0; i<= strLoc; i++){
				//temporary string that holds a number, new one for each
				//If the character isn't a colon, we haven't gotten 3 int values, and character isn't eof, add to tempStringNum
				if (recString[i]!=':' && hms<3 && recString[i]!='\0'){
					tempStringNum[placement++]=recString[i];
					//If haven't gotten 3 int's and character is colon, store int(stringNum) into tempNum[<current time param>]
				} else if (hms<2 && recString[i] == ':') {
					tempNum[hms++] = atoi(tempStringNum);
					for (int j=0; j <= placement; j++){tempStringNum[j]=NULL;}	//reset the string
					placement=0;												//reset placement
					//If we haven't found 3 int values and current Location is at end, and end char is null, save as second
				} else if (hms<3 && (recString[i]=='\0' || i==strLoc)){
					tempNum[hms] = atoi(tempStringNum);
					hms++;	//goes to three, nothing should happen.
				}//end else if
			}//end for
			//Make sure the settings are okay before setting the time. If and issue and number of errors is >3, ask user for time
			if (tempNum[0]/24==0 && tempNum[1]/60==0 && tempNum[2]/60==0){
				currentTime.setTime(tempNum[0],tempNum[1],tempNum[2]);
				saveDateTime_eeprom(fTrue,fFalse);
				PrintWAVR(recString);
				if (justStarted){justStarted=fFalse;}	//lower just started flag
			} else {
				PrintWAVR("ACKBAD.");
			}	
			state=4;
			break;			
			}//end case	
		case 4:{
			state=0;
			//Do anythingi extra here for a reset.
			flagUARTWAVR=fFalse;
			__enableLevel1INT();
		}
		default: {state=0; flagUARTWAVR=fFalse; break;}
			
	}//end switch	
	/*----------------------------------------------------------END STATE MACHINE--------------------------------------------------------------------------*/
		
	} while (flagUARTWAVR); //end do while						
}//end function
	
	

/*************************************************************************************************************/

//To print to WAVR, cariable needs to be false. Print to Bone requires WAVRorBone to be true
void printTimeDate(BOOL WAVRorBone, BOOL pTime,BOOL pDate){
	if (WAVRorBone){ //Printing to BeagleBone
	if (pTime){
		char tempTime[11];
		strcpy(tempTime,currentTime.getTime());
		PrintBone(tempTime);
		PutUartChBone('/');
	}
	if (pDate){
		char tempDate[17];
		strcpy(tempDate,currentTime.getDate());
		PrintBone(tempDate);
		PrintBone(".\0");
	}
} else { //Printing to GAVR
if (pTime){
	char tempTime[11];
	strcpy(tempTime,currentTime.getTime());
	PrintWAVR(tempTime);
	PutUartChWAVR('/');
}
if (pDate){
	char tempDate[17];
	strcpy(tempDate,currentTime.getDate());
	PrintWAVR(tempDate);
	PrintWAVR(".\0");
}
	}
}
/*************************************************************************************************************/
