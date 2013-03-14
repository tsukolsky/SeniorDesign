/*******************************************************************************\
| myUart.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 3/3/2013
| Last Revised: 3/14/2013
| Copyright of Todd Sukolsky
|================================================================================
| Description: This file conatians the UART0 and UART1 basic send and receive routines used 
|			by the Atmel family of microcontrollers
|--------------------------------------------------------------------------------
| Revisions: 3/3: Initial build
|			 3/4: Added Sending protocol to GAVR, added receive protocol from BeagleBone.
|			3/14: Double checked Bone receive and GAVR send logic. They should now be correct.
|				  Next step is to implements WAVR receive time/date from GAVR. Also need to add 
|				  timing logic in main to allow "Reminder" to GAVR that the watchdog needs the
|				  user date and time if it was asked for. 
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
extern BOOL flagReceivingBone, flagFreshStart, restart;
extern WORD globalADC, globalTemp;
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
	
	//Used for shutdown connection logic if there was a timeout in sending or receiving
	BOOL flagTimeout=fFalse;
	
	//Transmission protocol
	while (flagSendingGAVR && !flagTimeout){
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
				//Raise interrupts to GAVR for three ish clock cycles.
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
							//recString[strLoc++] = recChar;
							if (strLoc >= 39){strLoc = 0; noCarriage=fFalse; state=7;}
						}//end if-else
					}//end if-else	
				}//end while
				break;
				}//end case 1
			case 2: {
				if (!strcmp(recString,"ACKW.")){state=3;}
				else if (!strcmp(recString,"ACKGD.")){state=4;}
				else if (!strcmp(recString,"ACKGT.")){state=4;}
				else if (!strcmp(recString,"ACKGB.")){state=4;}
				else if (!strcmp(recString,"ACKBAD.")){state=6;}
				//send string case.
				else if (updatingGAVR && !strcmp(recString,sentString)){state=5;}		//they match, successful send.
				else if (updatingGAVR && strcmp(recString,sentString) && strcmp(recString,"ACKBAD.")){state=7;}	//string isnt the same as ACKBAD or what we sent.
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
					strcpy(sentString,"SYN");			//this is a syn, not ack to save logic in GAVR code. Can change if we want.
					strcat(sentString,currentTime.getTime());
					strcat(sentString,"/");	//add delimiter.
					strcat(sentString,currentTime.getDate());
					strcat(sentString,".\0");
					PrintGAVR("SYN");
					printTimeDate(fFalse,fTrue,fTrue);			//date is terminated by a . so don't need to send character
				}			
				//Reset the recString to receive the next ACK.
				for (int i=0; i<strLoc; i++){
					recString[i]=NULL;
				}
				//Reset the carriage feature, string location and go back to the receiving state.
				noCarriage=fTrue;
				strLoc=0;
				state=1;
				break;
				}//end case 3
			case 4:{
				//Successful communication with just flags
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
				//ACKBAD. Check the date and time to see if its okay. IF okay, go to state 7 to set waiting flag. otherwise set invalid time and close through state 5.
				BOOL dateOK = currentTime.checkValidity();
				if (dateOK){state=7;}
				else {state=5; flagInvalidDateTime=fTrue;}
				break;
				}//end case 6
			case 7:{
				//Got the wrong ACK back, or invalid ACK. Wait for next cycle then resend. Keep all the flags the same
				flagWaitingToSendGAVR=fTrue;
				flagSendingGAVR=fFalse;
				flagTimeout=fFalse;
				state=0;
				break;
				}//end case 7
			default:{state=0; flagSendingGAVR=fFalse; noCarriage=fFalse; flagTimeout=fFalse;break;}
		}//end switch
	}//end while
	
	//If there was a timeout and the wiating flag has not been set yet, make sure waiting flag.
	if (noCarriage || flagTimeout){
		flagWaitingToSendGAVR=fTrue;
	}
	
	//If we aren't waiting for the next round, don't reset the flags. If we are waiting, just reset the waiting flag. Like a stack popping
	if (!flagWaitingToSendGAVR){
		flagUserDate=fFalse;
		flagUserTime=fFalse;
		flagUpdateGAVRDate=fFalse;
		flagUpdateGAVRTime=fFalse;		
	} else {flagWaitingToSendGAVR=fFalse;}
}//end function 	
	
/*************************************************************************************************************/
void ReceiveBone(){
	volatile static unsigned int state=0;
	char recChar, recString[20];
	volatile unsigned int strLoc=0;
	BOOL noCarriage=fTrue;
	
	while (flagReceivingBone){
			/*************************************************************BEGIN STATE MACHINE************************************************/
			/* State 0: Get the first character that was loaded in the buffer. If ., go to ACKERROR state, else go to state 1				*/
			/* State 1: Get the rest of a string. If timeout, do nothing. Otherwise assemble. If overflow, go to ACKERROR, else go to state2*/
			/* State 2: String parsing. See what it's asking for/giving. If command, return what it wants. If time, try and set the time.	*/
			/*			If successful, go to successful transmission state 3, set corresponding flags based on WAVR state (startup/restart) */
			/*			, else if not good string go to ACKBAD state 4.																		*/
			/* State 3: Successful receive state. Print ACK<string> and then exit the loop													*/
			/* State 4: ACKBAD, string it sent was not valid. Reply and exit loop.															*/
			/* State 5: ACKERROR, invalid string or overflow. Say error then exit.															*/
			/* State 6: Graceful exit. Exit from a command like adc or temp.																*/
			/********************************************************************************************************************************/
			
			switch(state){
				case 0:{
					strLoc=0;
					recChar = UDR0;
					if (recChar=='.'){
						state=5;
					} else  {recString[strLoc++]=recChar; state=1;}	
					break;				
					}//end case 0
				case 1:{
					while (noCarriage && flagReceivingBone){	//while there isn't a timeout and no carry
						while (!(UCSR1A & (1 << RXC0)) && flagReceivingBone);		//get the next character
						if (!flagReceivingBone){state=0; break;}					//if there was a timeout, break out and reset state
						recChar=UDR0;
						recString[strLoc++]=recChar;
						if (recChar == '.'){recString[strLoc]='\0'; noCarriage=fFalse; state=2;}
						else{
							//recString[strLoc++]=recChar;
							if (strLoc >= 19){state=5;noCarriage=fFalse;}
						}//end if-else
					}//end while
					break;
					}//end case 1
				case 2:{
					if (!strcmp(recString,"date.")){printTimeDate(fTrue,fFalse,fTrue); state=6;}
					else if (!strcmp(recString,"time.")){printTimeDate(fTrue,fTrue,fFalse);state=6;}
					else if (!strcmp(recString,"both.")){printTimeDate(fTrue,fTrue,fTrue);state=6;}
					else if (!strcmp(recString,"save.")){saveDateTime_eeprom(fTrue,fFalse);PrintBone(recString);state=6;}
					else if (!strcmp(recString,"adc.")){char tempChar[7]; utoa(globalADC,tempChar,10); tempChar[6]='\0'; PrintBone(tempChar);state=6;}
					else if (!strcmp(recString,"temp.")){char tempChar[7]; utoa(globalTemp,tempChar,10); tempChar[6]='\0'; PrintBone(tempChar);state=6;}
					else if (recString[2] == ':'){//valid string. Update the time anyways. Comes in every 20 minutes or so...
						BOOL success=currentTime.setTime(recString);
						if (success){state=3;}
						else {state=4;}
						
						//Decide what I need to save and which flags need to go up.	
						if (success && !restart && !flagFreshStart){saveDateTime_eeprom(fTrue,fFalse); flagUpdateGAVRTime=fTrue;}
						else if (success && !restart && flagFreshStart){saveDateTime_eeprom(fTrue,fFalse); flagUpdateGAVRTime=fTrue; flagUserDate=fTrue;}
						else if (success && restart){saveDateTime_eeprom(fTrue,fFalse); flagUpdateGAVRDate=fTrue; flagUpdateGAVRTime=fTrue;;}
						else if (!success && restart){flagUpdateGAVRTime=fTrue; flagUpdateGAVRDate=fTrue;}	//sends eeprom time and date
						else if (!success && flagFreshStart && !restart){flagUserTime=fTrue; flagUserDate=fTrue;} //need to get user time and date
						else;
						//Reset flags for startup
						if (restart){restart=fFalse;}
						if (flagFreshStart){flagFreshStart=fFalse;}	
					} else if (!strcmp(recString,"SYNNONE.")){state=3;}	
					else {state=5;}						
					break;
					}//end case 2
				case 3:{
					//Successful receive state
					PrintBone("ACK");
					PrintBone(recString);
					state=0;
					flagReceivingBone=fFalse;
					break;
					}//end case 3
				case 4:{
					PrintBone("ACKBAD.");
					flagReceivingBone=fFalse;
					state=0;
					break;
					}//end case 4
				case 5:{
					PrintBone("ACKERROR.");
					flagReceivingBone=fFalse;
					state=0;
					break;
					}//end case 5
				case 6:{
					flagReceivingBone=fFalse;
					state=0;
					break;
					}//end case 6
				default:{flagReceivingBone=fFalse; state=0;break;}
			}//end switch
	}//end while(flagUARTbone)	
}//end ReceiveBone()

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
