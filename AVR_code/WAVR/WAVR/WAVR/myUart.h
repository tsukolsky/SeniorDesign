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



//Declare external flags and clock.
extern BOOL flagSendingGAVR;
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

void sendToGAVR(){
	cli();
	//__disableLevel1INT();
	sei();
	
	//Declare variables to be used.
	volatile static unsigned int state=0;
	volatile BOOL noCarriage=fTrue;
	char recChar, recString[20];
	unsigned int strLoc=0;
	
	//Going to be a global
	BOOL flagWaitingToSendGAVR=fFalse;
	
	//Transmission protocol
	while (flagSendingGAVR){
		/*************************************BEGIN STATE MACHINE**************************************************/
		/* */
		/* */
		//Currently state 8 is for timeout, state 9 is for overflow/going to wait 3 seconds
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
					if (!flagSendingGAVR){state=8; break;}
					else {
						recChar=UDR1;
						recString[strLoc++]=recChar;
						if (recChar=='.'){recString[strLoc++]='\0'; state=2;}
						else {
							recString[strLoc++] = recChar;
							if (strLoc >= 19){strLoc = 0; noCarriage = fFalse; flagWaitingToSendGAVR=fTrue;state=9;}
						}//end if-else
					}//end if-else	
				}//end while
				break;
				}//end case 1
			case 2: {
				if (strcmp(recString,"ACKW.")){state=3;}
				else if (strcmp(recString,"ACKGD")){state=4;}
				else if (strcmp(recString,"ACKGT")){state=5;}
				else if (strcmp(recString,"ACKGB")){state=6;}
				//send string case.
				else if (strcmp(recString,"  ")){/**do something**/;}
				else{flagWaitingToSendGAVR=fTrue;state=9;}
				break;
				}//end case 2
			case 3:{
				if (fTrue){
					PrintGAVR("SYNGD.");
				} else if (fFalse){
					//Print this;
				} else;				
				for (int i=0; i<strLoc; i++){
					recString[i]=NULL;
				}
				noCarriage=fTrue;
				strLoc=0;
				state=1;
				break;
				}//end case 3
			case 4:{
				
				
				}//end case 4	
			
		
		
		}//end switch
		
		
		
		
	}//end while
	
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
}
	}
}
/*************************************************************************************************************/
