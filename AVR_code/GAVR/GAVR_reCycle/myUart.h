/*******************************************************************************\
| myUart.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 3/3/2013
| Last Revised: 4/6/2013
| Copyright of Todd Sukolsky
|================================================================================
| Description: This file conatians the UART0 and UART1 basic send and receive routines used 
|			by the Atmel family of microcontrollers
|--------------------------------------------------------------------------------
| Revisions: 3/3: Initial build
|			3/14: Slight changes to implemntation of GAVR->WAVR responses for 
|				  WAVR requests.
|			3/27: Updated flags that were changed in "main". Working  on protocols to
|				  and from WAVR. Added SendWAVR() and some extra flags to help get things
|				  correct like "flagInvalidDateTime and flagGetWAVRtime. Next is SendBone(), 
|				  but first the startup procedure is going to be nailed down with trip statistics
|				  saved in EEPROM that way the Procedure knows what to pull,send, etc.
|			4/3-4/6- See "GAVR_reCycle.cpp" revision for '4/5-4/6'
|================================================================================
| *NOTES:
\*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stdtypes.h"

//Declare external flags and clock.
extern BOOL flagReceiveWAVR, flagReceiveBone, flagSendWAVR, flagSendBone, flagGetUserClock, flagWAVRtime, flagGetWAVRtime,justStarted, flagUpdateWAVRtime,flagWaitingForWAVR;
extern BOOL flagInvalidDateTime;
extern myTime currentTime;

//Forward Declaration
void printTimeDate(BOOL WAVRorBone, BOOL pTime, BOOL pDate);

/*************************************************************************************************************/
void Wait_ms(unsigned int delay)
{
	volatile int i;

	while(delay > 0){
		for(i = 0; i < 200; i++){
			asm volatile("nop");
		}
		delay -= 1;
	}
}
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
		Wait_ms(350);
		PutUartChWAVR(string[i++]);
	}
}
/*************************************************************************************************************/

//Only two things should ever be transmitted to this device. The Time or the request for the user to set date.If the
//request for user to set date happens
void ReceiveWAVR(){
	sei();	//make sure clock is still functioning, everything else should be killed by this point though.
	
	//Declare variables to be used.
	volatile unsigned int state=0;
	char recString[40];
	char recChar;
	int strLoc=0;
	BOOL noCarriage=fTrue;
	
	while (flagReceiveWAVR){

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
			if (recChar=='.'){PrintWAVR("E.");state=4;}
			else {recString[strLoc++]=recChar; state=1;}
			break;
			}
		case 1: {
			while (noCarriage && flagReceiveWAVR){
				Wait_ms(100);		//WAVR is sending at increment of 300ms
				while (!(UCSR1A & (1 << RXC1)) && flagReceiveWAVR);	//wait for this sucker to come in
				if (!flagReceiveWAVR){PrintBone("TimeoutG-"); PrintBone(recString);state=0; break;}				//there was a timeout in that receive of character
				recChar=UDR1;
				recString[strLoc++]=recChar;
				if (recChar=='.'|| recChar=='\0'){recString[strLoc]='\0'; noCarriage=fFalse; state=2;}
				else if (strLoc >= 39){strLoc = 0; noCarriage = fFalse; flagReceiveWAVR=fFalse; PrintWAVR("E."); state=0;}
				else; //end if-elseif-else
			}//end while noCarriage
			break;
			}//end case 1
		case 2: {
			if (!strncmp(recString,"G.",3)){			//G is for get
				state=4; 
				PrintBone("Was asked for date.");
				Wait_ms(100);
				PrintWAVR("G."); 
				flagGetUserClock=fTrue;					//alert that we need to get the string
				if (flagWAVRtime){flagWAVRtime=fFalse; flagUpdateWAVRtime=fFalse;}	//let the WAVR flag go down so that if a string comes in, we can allocate it. Mkae sure we don't try and send our time right away.
			} else if ((recString[2]==':') != (recString[3]==':')){state=3;}	//go parse the string
			else {PrintWAVR("E.");state=4;PrintBone("Error receiving-");PrintBone(recString);}					//E is for error
			break;
			}
		case 3: {
			//Parse the string...if you want to limit whether WAVR can control GAVR, just add an if statement here with a locking flag if the user actually does set the time.
			//Go through the string and parse for the time. Must go through the time to get the date.
			BOOL successTime=fFalse, successDate=fFalse;			//whether or not we have successfully parsed string
			int counter=1;											//time starts at 1
			int tempNum[3]={0,0,0}, tempNum1[3]={0,0,0},dmy=0, hms=0, placement=0;
			char tempStringNum[5];
			
			//Parse the string for the time. Always looks for the time. If not end of string or '/' indicating start of date, continue
			while (recString[counter] != '/' && recString[counter] != '\0'){
				//If the character isn't a colon, we haven't gotten 3 int values add to tempStringNum
				if (recString[counter]!=':' && hms<3){
					tempStringNum[placement++]=recString[counter];
					//If haven't gotten 3 int's and character is colon, store int(stringNum) into tempNum[<current time param>]
				} else if (hms<2 && recString[counter] == ':') {
					tempNum[hms++] = atoi(tempStringNum);
					for (int j=0; j <= placement; j++){tempStringNum[j]=NULL;}	//reset the string
					placement=0;												//reset placement
					//If nothing else, somethign is wrong but it won't matter because we'll eventually hit \0 and exit with ACKBAD
				} else;
				counter++;
			}//end while
			//Found a '/', assign tempNum otherwise exit with ACKBAD
			if (recString[counter] == '/'){
				tempNum[hms] = atoi(tempStringNum);
				successTime=fTrue;
			} else {
				state=5;
			}
			
			//If flag for Date is set, then parse the string and do something with it.
			//Now get the date. have to null the tempStringNum
			for (int j=0; j <= placement; j++){tempStringNum[j]=NULL;}	//reset the string
			placement=0;
			counter++;	//get past the '/'
		
			//Loop through the string. If not end of file and counter isn't end of string, and not terminator '.', continue
			while (recString[counter] != '.' && recString[counter] != '\0' && counter != strLoc){
				//If char isn't sepaerator or end of string of dmy has been hit, add to buffer
				if  (recString[counter] != ',' && dmy < 3){
					tempStringNum[placement++]=recString[counter];
					//If a comma was found, need to store that sucker in the tempNum1[x]. dmy needs to be 0 or 1 aka month or day.
				} else if (dmy<2 && recString[counter]==','){
					tempNum1[dmy++] = atoi(tempStringNum);
				for (int j=0; j <= placement; j++){tempStringNum[j]=NULL;}
				placement=0;
				} else;
				counter++;
			}//end while
	
			//Assign last date if the reason we broke the while loop was a '.'. If not, ACKBAD and exit.
			if (recString[counter] == '.'){
				tempNum1[dmy] = atoi(tempStringNum);
				successDate=fTrue;
			} else {//something in the string was wrong, ACKBAD and then exit
				state=5;
			}//end if-else
			
			//Make sure the settings are okay before setting the time. If and issue and number of errors is >3, ask user for time
			if (successDate && successTime){
				currentTime.setTime(tempNum[0],tempNum[1],tempNum[2]);
				currentTime.setDate(tempNum1[0],tempNum1[1],tempNum1[2]);
				saveDateTime_eeprom(fTrue,fTrue);
				flagGetUserClock=fFalse;
				flagWAVRtime=fTrue;
				
				//send ACK
				PrintBone("Set the time.");
				recString[0]='A';
				PrintWAVR(recString);
				
				state=4;
			} else {
				//Here is a choice: Do we watn  to user to indicate the time is wrong, or just make them set it? Doesn't really matter.
				flagGetUserClock=fTrue;
				flagWAVRtime=fFalse;
				PrintBone("Bad time");
				state=5;
			}
			//Exit to appropriate location.
			break;			
			}//end case	3
		case 4:{
			PrintBone("Exiting.");
			state=0;
			//Do anythingi extra here for a reset.
			flagReceiveWAVR=fFalse;
			for (int i=0; i<=strLoc; i++){recString[i]=NULL;}
			
			break;
		}
		case 5:{
			//ACKBAD case
			Wait_ms(150);
			PrintWAVR("B.");
			flagReceiveWAVR=fFalse;
			for (int i=0; i<=strLoc; i++){recString[i]=NULL;}
			state=0;
			break;
		}
		default: {state=0; flagReceiveWAVR=fFalse; break;}
			
	}//end switch	
	/*----------------------------------------------------------END STATE MACHINE--------------------------------------------------------------------------*/
		
	} //end while						
}//end function
	
	
/*************************************************************************************************************/
void ReceiveBone(){
	unsigned int state=0;
	char recChar, recString[40];
	unsigned int strLoc=0;
	BOOL noCarriage=fTrue;
	
	while (flagReceiveBone){
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
		/* State 7: Parse the input string for the time and date. Should be time(:)'/'date(,) where : and , are the delimiters. Term by */
		/*			'.'																													*/
		/********************************************************************************************************************************/
		
		switch(state){
			case 0:{
				//Get the first character. If a '.', exit to bad state.
				strLoc=0;
				recChar = UDR0;
				if (recChar=='.'){
					state=3;
				} else  {recString[strLoc++]=recChar; state=1;}
				break;
			}//end case 0
			case 1:{
				while (noCarriage && flagReceiveBone){	//while there isn't a timeout and no carry
					Wait_ms(80);
					while ((!(UCSR0A & (1 << RXC0))) && flagReceiveBone);		//get the next character
						if (!flagReceiveBone){state=4;break;}					//if there was a timeout, break out and reset state
						recChar=UDR0;
						recString[strLoc++]=recChar;
						if (recChar == '.' || recChar=='\0'){recString[strLoc]='\0'; noCarriage=fFalse; state=2;}
						else if (strLoc >= 39){state=3;noCarriage=fFalse;}
						else; //end if-elseif-else
					}//end while
					break;
					}//end case 1
				case 2:{
					if (!strncmp(recString,"d.",2)){printTimeDate(fTrue,fFalse,fTrue); state=4;}
					else if (!strncmp(recString,"t.",2)){printTimeDate(fTrue,fTrue,fFalse);state=4;}
					else if (!strncmp(recString,"b.",2)){printTimeDate(fTrue,fTrue,fTrue);state=4;}
					else if (!strncmp(recString,"s.",2)){saveDateTime_eeprom(fTrue,fFalse);PrintBone(recString);state=4;}		
					else {state=3;}						
					break;
					}//end case 2
				case 3:{
					//Didn't get a good ack or there was an error.
					Wait_ms(100);
					PrintBone("E.");
					flagReceiveBone=fFalse;
					for (int i=0; i<strLoc; i++){recString[i]=NULL;}
					state=0;
					break;
					}//end case 5
				case 4:{
					//Graceful exit.
					flagReceiveBone=fFalse;
					for (int i=0; i<strLoc; i++){recString[i]=NULL;}
					state=0;
					break;
					}//end case 6						
				default:{flagReceiveBone=fFalse; state=0;break;}
			}//end switch
	}//end while(flagUARTbone)	
}//end ReceiveBone()
/*************************************************************************************************************/
void SendWAVR(){
	//Declare variables to be used.
	volatile static unsigned int state=0;
	volatile BOOL noCarriage=fTrue;
	char recChar, recString[40], sentString[40];
	unsigned int strLoc=0;
	
	//Set Flag to begin
	flagSendWAVR=fTrue;
	
	while (flagSendWAVR){		
	/***************************************************************NEW METHOD-STATE MACHINE**********************************************************/
	/** State 0: Send interrupt to WAVR that we are going to communicate. Go to state 1.															**/
	/** State 1: Put together the incoming UART string. If a '.' is found, go to state 2 to parse the string. If overflow, exit to ACKERRO case 7.	**/
	/** State 2: Find out what was said. If initial ACK, go to state 3 to figure out waht to send. If ACKNEED, the WAVR now knows we need the clock.**/
	/**			 If it was the clock string we sent, but with ACK, exit. If ACKNO, we tried to send date/time, shouldn't have, ie WAVR has valid	**/
	/**			 time so set flag to get the time and exit. If ACKBAD go to state 4, else state 5 whici is ACKERROR case/timeout					**/
	/** State 3: If we need WAVRtime, send SYNNEED. If we are updating the WAVR, send the time and allocate the compare string for the ACK.			**/
	/**			 Re-initialize variables then return to state 1 to find out waht ACK comes in.														**/
	/** State 4: ACKBAD came in, make sure our date and time are valid. if not, set flag and exit. If they are valid, go to state 5 to enable a		**/
	/**			 resend of information.																												**/
	/** State 5: There was either ACKERROR, a timeout, or an error with validity that was wrong. Need to resend. Raise flag that to wait, kill send **/
	/**			 flag to allow exiting of main while loop. At end of function, re-enable flagSendWAVR for the resend.								**/
	/** State 6: Graceful exit																														**/
	/** Defaulte: Set state, exit.																													**/
	/*************************************************************************************************************************************************/
		
		switch (state){
			case 0:{
				//Raise Interrupts
				Wait_ms(1000);
				prtWAVRINT |= (1 << bnWAVRINT);
				for (int i=0; i<2; i++){asm ("nop");}
				prtWAVRINT &= ~ (1 << bnWAVRINT);
				state=1;
				Wait_ms(1000);
				break;
				}//end case 0			
			case 1:{
				Wait_ms(100);
				//Put together the string that is being received
				while (noCarriage && flagSendWAVR){
					Wait_ms(100);
					PrintBone("WaitingG.");
					while (!(UCSR1A & (1 << RXC1)) && flagSendWAVR);
					if (!flagSendWAVR){PrintBone("TimeoutG.");PrintBone(recString);state=0; break;}	//there was a timeout, don't do anything else. Flags aren't reset.
					else {
						recChar=UDR1;
						unsigned int temp=(int)recChar;
						recString[strLoc++]=recChar;
						if (temp==46){recString[strLoc]='\0'; state=2;}
						else if (strLoc >= 39){strLoc=0; noCarriage=fFalse; state=5;}			//Something went wrong putting together the message/ACK
						else;
					}					
				}//end while (noCarriage && flagSendWAVR)
				break;
				}//end case 1
			case 2:{
				if (!strncmp(recString,"A.",2)){state=3;PrintBone("Got ACK.");}	//Ack
				else if (!strncmp(recString,"N.",2)){PrintBone("Need.");/*Exit gracefully*/state=5; flagWaitingForWAVR=fTrue; flagGetWAVRtime=fTrue; flagUpdateWAVRtime=fFalse;}		//GAVR knows we need time, if it has it it will tell us. GetWAVRtime flag is already set, make sure it's up though.
				else if (!strcmp(recString,sentString)){state=6; flagUpdateWAVRtime=fFalse; flagGetWAVRtime=fFalse;}	//Got the correct user clock, we can kill that signal now since it was set.
				else if (!strncmp(recString,"NO.",3)){state=6; flagGetWAVRtime=fTrue; flagUpdateWAVRtime=fFalse;}	//We shouldn't have sent anything, just exit. Maybe we should ask for the WAVR time now.
				else if (!strncmp(recString,"B.",2)){state=4;} //check validitity of date and time
				else{state=5;PrintBone("Error.");}	//ACKERROR case.
				break;
				}//end case 2
			case 3:{	
				Wait_ms(100);		
				if (flagGetWAVRtime && !flagUpdateWAVRtime){//No real case that this would happen.
					PrintWAVR("N.");
				} else if (flagUpdateWAVRtime && !flagGetWAVRtime){	//The WAVR needed the time, send it back
					PrintBone("Giving time.");
					strcpy(sentString,"A");
					strcat(sentString,currentTime.getTime());
					strcat(sentString,"/");
					strcat(sentString,currentTime.getDate());
					strcat(sentString,".\0");
					PrintWAVR("S");
					printTimeDate(fFalse,fTrue,fTrue);		//prints time and date to WAVR				
				} else if (flagUpdateWAVRtime && flagGetWAVRtime){
					PrintWAVR("N.");
					flagUpdateWAVRtime=fFalse;
				} else if (!flagGetUserClock && !flagGetWAVRtime){
					PrintWAVR("N.");
					flagGetWAVRtime=fTrue;
				} else;
			
				//Reset variables for next reception
				for (int i=0; i<=strLoc; i++){recString[i]=NULL;}
				strLoc=0;
				recChar=NULL;
				noCarriage=fTrue;
				state=1;			//go listen for what we want.
				break;	
				}//end case 3
			case 4:{
				//ACKBAD
				//Check validity of date and time, then exit
				BOOL dateOK = currentTime.checkValidity();
				if (!dateOK){flagInvalidDateTime=fTrue;}
				state=6;
				break;	
				}//end case 4
			case 5:{
				//ACKERROR or timeout, just exit and try again without resetting flags. Same as case 6...
				state=0;
				flagSendWAVR=fFalse;
				for (int i=0; i<=strLoc; i++){recString[i]=NULL;}
				break;
				}//end case  5
			case 6:{
				//Graceful exit
				state=0; 
				flagSendWAVR=fFalse;
				for (int i=0; i<=strLoc; i++){recString[i]=NULL;}
				break;
				}//end case 6
			default:{state=0; flagSendWAVR=fFalse; break;}
	
		}//end switch
	}//end while(flagSendWAVR)	
}//end function
/*************************************************************************************************************/
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
	}//end if-else
}//end function
/*************************************************************************************************************/
