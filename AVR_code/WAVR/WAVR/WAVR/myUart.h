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
| *NOTES: This is the UART for the WAVR.
\*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stdtypes.h"
#include "ATmegaXX4PA.h"	//has interrupt pins

//Implementations for interrupt resets and sets

//Forward Declarations
void printTimeDate(BOOL WAVRorBone=fFalse, BOOL pTime=fTrue,BOOL pDate=fTrue);


//Declare external flags and clock.
extern BOOL flagSendingGAVR, flagUserDate, flagUserTime, flagUpdateGAVRTime, flagUpdateGAVRDate, flagInvalidDateTime, flagWaitingToSendGAVR;
extern myTime currentTime;

#define updatingGAVR (flagUpdateGAVRDate || flagUpdateGAVRTime)

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

void PutUartChGAVR(char ch){
	while (!(UCSR1A & (1 << UDRE1)));
	UDR1=ch;
}
/*************************************************************************************************************/

void PrintGAVR(char string[]){
	BYTE i=0;
	while (string[i]){
		PutUartChGAVR(string[i++]);
	}
}
/*************************************************************************************************************/

void sendGAVR(){
	//Declare variables to be used.
	volatile static unsigned int state=0;
	volatile BOOL noCarriage=fTrue;
	char recChar, recString[40], sentString[40];
	unsigned int strLoc=0;
	
	//Going to be a global
	BOOL flagTimeout=fFalse;
	
	//Transmission protocol
	while (flagSendingGAVR){
		/*************************************BEGIN STATE MACHINE*****************************************************/
		/* State 0: Send interrupt to GAVR then move to state 1														 */
		/* State 1: Receive a string and put it together. Add the final . onto the string also. String terminated by */
		/*			a '.' and will go to state 2 if successfully captures a string, state 7 if there was an issue.	 */
		/* State 2: Parse the string we received to see what it says. If initial ack, go to state 3 and print what we*/
		/*			want to send, otherwise check it against the ACK we just sent. (assuming D doesn't become D etc.)*/
		/*			Move to state 4 if it was just to set flags, if ACKBAD go to state 6 to check validity of date   */
		/*			and/or time. If it's an ack of the time string we sent it, make sure it matches. If so, state 5, */
		/*			otherwise go to state 7 because there was a transmission error one way or the other.			 */
		/* State 3: If we need to have user input date and time, send the SYN for it. If we are sending date and time*/
		/*			send it and wait to see if the response is what we sent. Goes to state 1. Reset string receivestf*/
		/* State 4: We send something just to update flags, if we got the correct SYN then send SYNDONE and exit to  */
		/*			state 5 which is successful connection close.												     */
		/* State 5: Successful transmission and reception. Kill sending flag, reset state, exit.					 */
		/* State 6: ACKBAD received, check to see if we have an invalid date/time in our clock or it was just noisy  */
		/*			and had error in transmission/reception. If valid, go to waiting state. Else exit and set flag.	 */
		/* State 7: Got the wrong ack for something, set a flag to let the WAVR do its thing then try again.		 */
		/*************************************************************************************************************/
		switch (state){
			case 0: {
				prtGAVRINT |= (1 << bnGAVRINT);
				for (int i=0; i<2; i++){asm volatile("nop");}
				prtGAVRINT &= ~(1 << bnGAVRINT);
				state=1;
				break;
			}//end case 0
			case 1: {
				while (noCarriage && flagSendingGAVR){
					while (!(UCSR1A & (1 << RXC1)) && flagSendingGAVR);
					if (!flagSendingGAVR){state=0; flagTimeout=fTrue; break;}
					else {
						recChar=UDR1;
						recString[strLoc++]=recChar;
						if (recChar=='.'){recString[strLoc++]='\0'; state=2;}
						else {
							recString[strLoc++] = recChar;
							if (strLoc >= 39){strLoc = 0; noCarriage=fFalse; state=7;}
						}//end if-else
					}//end if-else	
				}//end while
				break;
				}//end case 1
			case 2: {
				if (!strcmp(recString,"ACKW.")){state=3;}
				else if (!strcmp(recString,"ACKGD")){state=4;}
				else if (!strcmp(recString,"ACKGT")){state=4;}
				else if (!strcmp(recString,"ACKGB")){state=4;}
				else if (!strcmp(recString,"ACKBAD")){state=6;}
				//send string case.
				else if (updatingGAVR && !strcmp(recString,sentString)){state=5;}
				else if (updatingGAVR && strcmp(recString,sentString)){state=7;}
				else{state=7;} //invalid ack
				break;
				}//end case 2
			case 3:{
				if (flagUserDate&&!flagUserTime){
					PrintGAVR("SYNGD.");
				} else if (!flagUserDate&&flagUserTime){
					PrintGAVR("SYNGT.");
				} else if (flagUserTime&&flagUserDate){
					PrintGAVR("SYNGB.");
				} else;
				
				//If we are updating the gavr, send the time and date together regardless. preface with SYN
				if (updatingGAVR && !(flagUserDate || flagUserTime)){
					strcpy(sentString,"SYN");
					strcat(sentString,currentTime.getTime());
					strcat(sentString,currentTime.getDate());
					strcat(sentString,".\0");
					PrintGAVR("SYN");
					printTimeDate(fFalse,fTrue,fTrue);			//date is terminated by a . so don't need to send character
				}				
				for (int i=0; i<strLoc; i++){
					recString[i]=NULL;
				}
				noCarriage=fTrue;
				strLoc=0;
				state=1;
				break;
				}//end case 3
			case 4:{
				//jSuccessful communication with just flags
				PrintGAVR("SYNDONE.");	//end the communication
				state=5;
				break;				
				}//end case 4	
			case 5:{
				//Successful communications overall
				flagSendingGAVR=fFalse;
				flagWaitingToSendGAVR=fFalse;
				flagTimeout=fFalse;
				state=0;
				break;
				}//end case 5
			case 6:{
				//ACKBAD. Check the date and time to see if its okay.
				BOOL dateOK = currentTime.checkValidity();
				if (dateOK){state=7;}
				else {state=0;flagSendingGAVR=fFalse; flagInvalidDateTime=fTrue;}
				break;
				}//end case 6
			case 7:{
				//Got the wrong ACK back, or invalid ACK. Wait for next cycle then resend. Keep all the flags the same
				flagWaitingToSendGAVR=fTrue;
				flagSendingGAVR=fFalse;
				state=0;
				break;
				}//end case 7
			default:{state=0; flagSendingGAVR=fFalse; noCarriage=fFalse; flagTimeout=fFalse;break;}
		}//end switch
	}//end while
	if (noCarriage || flagTimeout){
		flagWaitingToSendGAVR=fTrue;
	}
	//If we aren't waiting for the next round, don't reset the flags.
	if (!flagWaitingToSendGAVR){
		flagUserDate=fFalse;
		flagUserTime=fFalse;
		flagUpdateGAVRDate=fFalse;
		flagUpdateGAVRTime=fFalse;		
	}
	
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
			PutUartChGAVR('.');
		}
	} else { //Printing to GAVR
		if (pTime){
			char tempTime[11];
			strcpy(tempTime,currentTime.getTime());
			PrintGAVR(tempTime);
			PutUartChGAVR('/');
		}
		if (pDate){
			char tempDate[17];
			strcpy(tempDate,currentTime.getDate());
			PrintGAVR(tempDate);
			PutUartChGAVR('.');
		}
	}
}
/*************************************************************************************************************/
