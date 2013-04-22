/*******************************************************************************\
| gavrUart.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 3/3/2013
| Last Revised: 4/18/2013
| Copyright of Todd Sukolsky and Boston University ECE Senior Design Team ReCycle, 2013
|================================================================================
| Description: This file conatians the uart protocols for the Graphics AVR
|	for the Senior Design project ReCycle
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
|			4/7-  In SendWAVR(), in receiving a message there was no flag set for "noCarriage=fFalse" so the
|				  program kept looping. Oy vey, fixed that. Added extra flag 'flagNewTripStartup' that deals
|				  with trip data syncrhonization. (2) Added "SendBone" routine that either tells bone to delete
|				  one of it's trips, send it's trips over to view, or offload a trip to the bone. HTTP1.0, only
|				  one request/tirp at a time and then connection is reset.
|			4/8- Uart transmission from bone and to/from WAVR works. Had to tweak flags.
|			4/13- Added race-condition fix for incoming time from WAVR that is blank, example is when there is a memory
|				error on the WAVR and it doesn't send the time when calling "printTimeDate". Added case for sending
|				to WAVR as well.
|			4/14- Tried to merge changes, had to tweak language. Slight change in ReceiveBone2 and SendBone. Flag
|			      manipulation and race condition needed to be addressed if there was an error sending trips and
|			      Bone returned an 'E.'. If it tries to send twice, it will just keep adding. Chagned it so that
|			      if bone resends the GAVR will CLEAR the USBtripsViewer vector and then receive the trips like it should.
|			      In SendBone(), tweaked small error in BOOL when exiting from offloading trip as well as what "sendingWhat"
|			      is required to access (char *) array for preface characters.
|			4/8- Uart transmission from bone and to/from WAVR works. Had to tweak flags. Revised SendBone to work
|				  with initial script, added ReceiveBone2() which communicates. Implements vector class.
|			4/15- Final tweaks to ReceiveBone2 (now ReceiveBone) protocol as well as SendBone. Counterfparts tweaked as well.
|				  No more USB inserted/ejected communication, dealt with in GPIO pin. See note 5 in main. Added UART2 functionality
|				  for GAVR to talk to bone if needed in PrintBone2 and PutUARTChar2 routines, ReceiveBone2, and printDateTime now
|				  has flag recognition for which UART to go off of. Changed name.
|			4/18- GAVR now sends BeagleBone the Trip number it is sending so the BeagleBone can move the appropriate GPS file.
|================================================================================
| Revisions Needed:
|		4/7: ReceiveBone2() needs to be completed.|
|			--Resolved 4/15
|================================================================================
| *NOTES: (1) Currently, if the Bone sends trips to the GAVR it will ALWAYS RECEIVE and update them.
\*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stdtypes.h"

//Declare external flags, clock and trip.
extern BOOL flagReceiveWAVR, flagReceiveBone, flagReceiveBone2, flagSendWAVR, flagSendBone, flagGetUserClock, flagWAVRtime, flagGetWAVRtime,justStarted;
extern BOOL flagInvalidDateTime, flagNewTripStartup, flagUpdateWAVRtime,flagWaitingForWAVR, flagTripOver;
extern BOOL flagOffloadTrip, flagNeedTrips, flagDeleteUSBTrip, flagDeleteGAVRTrip, flagHaveUSBTrips, flagViewGAVRtrips, flagViewUSBTrips;
extern myTime currentTime;
extern trip globalTrip;
extern myVector<char *> USBtripsViewer;
//extern char *actualTrips[20];
extern int whichTripToOffload, whichUSBtripToDelete,whichGAVRtripToDelete;
//extern BYTE numberOfUSBTrips;

//Forward Declaration
void printTimeDate(BOOL WAVRorBone, BOOL pTime, BOOL pDate);
void Wait_sec(unsigned int delay);
void Wait_ms(unsigned int delay);

/*************************************************************************************************************/
void Wait_sec(unsigned int delay){
	volatile int startingTime = currentTime.getSeconds();
	volatile int endingTime= (startingTime+delay)%60;
while (currentTime.getSeconds() != endingTime){asm volatile ("nop");}
}

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
void PutUartChBone2(char ch){
	while (!(UCSR2A & (1 << UDRE2)));
	UDR2=ch;
}
/*************************************************************************************************************/
void PrintBone2(char string[]){
	BYTE i=0;
	while (string[i]){
		PutUartChBone2(string[i++]);
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
	BYTE state=0,strLoc=0;
	char recString[40];
	char recChar;
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
				while (!(UCSR1A & (1 << RXC1)) && flagReceiveWAVR);	//wait for this sucker to come in
				Wait_ms(50);
				if (!flagReceiveWAVR){PrintBone2("TimeoutRG-"); PrintBone(recString);state=0; break;}				//there was a timeout in that receive of character
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
				PrintWAVR("G."); 
				flagGetUserClock=fTrue;					//alert that we need to get the string
				if (flagWAVRtime){flagWAVRtime=fFalse; flagUpdateWAVRtime=fFalse;}	//let the WAVR flag go down so that if a string comes in, we can allocate it. Mkae sure we don't try and send our time right away.
			} else if ((recString[2]==':') != (recString[3]==':')){state=3;}	//go parse the string
			else if (!strncmp(recString,"S.",2)){flagGetUserClock=fTrue;PrintWAVR(recString);PrintBone2("Got S without time, error correction.");state=4;}	//Error condition when the WAVR tries to send a time that really isn't there. Something went wrong in it's memory.			
			else {PrintWAVR("E.");state=4;}					//E is for error
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
				flagGetWAVRtime=fFalse;
				//If a new trip was just started, and we just booted the system and asked for the time, or have an update to the time, reset the start day of that trip.
				if (flagNewTripStartup){
					flagNewTripStartup=fFalse;
					globalTrip.setStartDate(tempNum1[0],tempNum1[1],tempNum1[2]);		//set the start date for the current trip, then be done with it.
				}
				
				//send ACK
				recString[0]='A';
				PrintWAVR(recString);
				state=4;
			} else {
				//Here is a choice: Do we watn  to user to indicate the time is wrong, or just make them set it? Doesn't really matter.
				flagGetUserClock=fTrue;
				flagWAVRtime=fFalse;
				state=5;
			}
			//Exit to appropriate location.
			break;			
			}//end case	3
		case 4:{
			state=0;
			//Do anythingi extra here for a reset.
			flagReceiveWAVR=fFalse;
			for (int i=0; i<=strLoc; i++){recString[i]=NULL;}
			break;
		}
		case 5:{
			//ACKBAD case
			PrintWAVR("B.");
			flagReceiveWAVR=fFalse;
			for (int i=0; i<=strLoc; i++){recString[i]=NULL;}
			state=0;
			break;
		}
		default: {state=0; flagReceiveWAVR=fFalse; break;}
			
	}//end switch	
	} //end while						
}//end function
/*************************************************************************************************************/
void SendWAVR(){
	//Declare variables to be used.
	BYTE state=0, strLoc=0;
	BOOL noCarriage=fTrue;
	char recChar, recString[40], sentString[40];
	
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
				prtWAVRINT |= (1 << bnWAVRINT);
				for (int i=0; i<2; i++){asm ("nop");}
				prtWAVRINT &= ~ (1 << bnWAVRINT);
				state=1;
				break;
				}//end case 0
			case 1:{
				//Put together the string that is being received
				while (noCarriage && flagSendWAVR){
					Wait_ms(50);
					while (!(UCSR1A & (1 << RXC1)) && flagSendWAVR);
					if (!flagSendWAVR){state=0; break;}					//there was a timeout, don't do anything else. Flags aren't reset.
					recChar=UDR1;
					recString[strLoc++]=recChar;
					if (recChar=='.' || recChar=='\0'){recString[strLoc]='\0'; noCarriage=fFalse; state=2;}			//(4/7 Revision)Forgot to add noCarriage=fFalse;
					else if (strLoc >= 39){strLoc=0; noCarriage=fFalse; state=5;}									//Something went wrong putting together the message/ACK
					else;
				}//end while (noCarriage && flagSendWAVR)
				break;
				}//end case 1
			case 2:{
				if (!strncmp(recString,"A.",2)){state=3;}	//Ack
				else if (!strncmp(recString,"N.",2)){/*Exit gracefully*/state=6; flagWaitingForWAVR=fTrue; flagGetWAVRtime=fFalse; flagUpdateWAVRtime=fFalse;}		//GAVR knows we need time, if it has it it will tell us. GetWAVRtime flag is already set, bring it down so we don't keep asking.
				else if (!strcmp(recString,sentString)){state=6; flagUpdateWAVRtime=fFalse; flagGetWAVRtime=fFalse;}	//Got the correct user clock, we can kill that signal now since it was set.
				else if (!strncmp(recString,"NO.",3)){state=6; flagGetWAVRtime=fFalse; flagUpdateWAVRtime=fFalse;}	//We shouldn't have sent anything, just exit. Maybe we should ask for the WAVR time now.
				else if (!strncmp(recString,"B.",2)){state=4;} //check validitity of date and time
				else if (!strncmp(recString,"S.",2)){state=6; flagUpdateWAVRtime=fFalse; flagGetWAVRtime=fTrue;}	//Error case/race-condition (4/13 Revision)
				else{state=5;}	//ACKERROR case.
				break;
				}//end case 2
			case 3:{	
				Wait_ms(100);		
				if (flagGetWAVRtime && !flagUpdateWAVRtime){//No real case that this would happen.
					PrintWAVR("N.");
				} else if (flagUpdateWAVRtime && !flagGetWAVRtime){	//The WAVR needed the time, send it back
					strcpy(sentString,"A");
					strcat(sentString,currentTime.getTime());
					strcat(sentString,"/");
					strcat(sentString,currentTime.getDate());
					strcat(sentString,".\0");
					PutUartChWAVR('S');
					printTimeDate(fFalse,fTrue,fTrue);		//prints time and date to WAVR				
				} else if (flagUpdateWAVRtime && flagGetWAVRtime){			//if both are set, there was an error. Rason on the side of user stupidity
					PrintWAVR("E.");
					flagUpdateWAVRtime=fFalse;
				} else if (!flagUpdateWAVRtime && !flagGetWAVRtime){		//if both aren't set, do same as if both are set.
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
void ReceiveBone2(){
	char recChar, recString[40];
	BYTE strLoc=0, state=0;
	BOOL noCarriage=fTrue;
	
	while (flagReceiveBone2){
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
				recChar = UDR2;
				if (recChar=='.'){
					state=3;
				} else {recString[strLoc++]=recChar; state=1;}
				break;
			}//end case 0
			case 1:{
				while (noCarriage && flagReceiveBone2){	//while there isn't a timeout and no carry
					while ((!(UCSR2A & (1 << RXC2))) && flagReceiveBone2);		//get the next character
					if (!flagReceiveBone2){break;}					//if there was a timeout, break out and reset state
					recChar=UDR2;
					recString[strLoc++]=recChar;
					if (recChar == '.' || recChar=='\0'){recString[strLoc]='\0'; noCarriage=fFalse; state=2;}
					else if (strLoc >= 39){state=3;noCarriage=fFalse;}
					else; //end if-elseif-else
				}//end while
				break;
				}//end case 1
			case 2:{
				if (!strncmp(recString,"b.",2)){printTimeDate(fTrue,fTrue,fTrue);state=4;}
				else if (!strncmp(recString,"hi.",3)){PrintBone2("Hello BeagleBone.");state=4;}
				else if (!strncmp(recString,"s.",2)){saveDateTime_eeprom(fTrue,fFalse);PrintBone2(recString);state=4;}
				else if (!strncmp(recString,"vg.",3)){PrintBone2(recString);flagViewGAVRtrips=fTrue;state=4;}
				else if (!strncmp(recString,"vu.",3)){
					if (flagHaveUSBTrips){
						PrintBone2(recString);
						flagViewUSBTrips=fTrue;
					} else {
						flagNeedTrips=fTrue;
						PrintBone2("NO USB TRIPS, getting them...");
					}
					state=4;
				} else if (!strncmp(recString,"new.",4)){PrintBone2(recString);flagTripOver=fTrue; state=4;}
				/********************************************offload trip*****************************************/
				else if (recString[0]=='o'){
					whichTripToOffload=atoi((const char *)recString[1]);
					if (whichTripToOffload>0 && whichTripToOffload<=numberOfGAVRtrips){
						flagOffloadTrip=fTrue;
					}else{
						flagOffloadTrip=fFalse;
						whichTripToOffload=-1;
						PrintBone2("No trip with that number.");
					}//end if valid trip.
					state=4;
				/********************************************delete USB/GAVR trip*********************************/
				}else if (recString[0]=='d'){
					if (recString[1]=='u'){
						whichUSBtripToDelete=atoi((const char *)recString[2]);
						if (flagHaveUSBTrips && whichUSBtripToDelete <=USBtripsViewer.size()){
							flagDeleteUSBTrip=fTrue;
							PrintBone2(recString);
						} else {
							flagDeleteUSBTrip=fFalse;
							whichUSBtripToDelete=-1;
							PrintBone2("Invalid USB trip.");
						}//end if-else valid number
					} else if (recString[1]=='g'){
						char *tempString;
						tempString[0]=recString[2];
						tempString[1]='\0';
						whichGAVRtripToDelete=atoi(tempString);
						if (whichGAVRtripToDelete>0 && whichGAVRtripToDelete<numberOfGAVRtrips){
							flagDeleteGAVRTrip=fTrue;
							PrintBone2(recString);
						} else {
							flagDeleteGAVRTrip=fFalse;
							whichGAVRtripToDelete=-1;
							PrintBone2("Invalid GAVR trip.");
							PutUartChBone2(recString[2]);
						}//end if valid gavr trip
					}
					else;//end if-elseif-else
					state=4;
				}				
				//else if (!strncmp(recString,""))		
				else {PrintBone2(recString);state=3;}						
				break;
				}//end case 2
			case 3:{
				//Didn't get a good ack or there was an error.
				Wait_ms(100);
				flagReceiveBone2=fFalse;
				for (int i=0; i<strLoc; i++){recString[i]=NULL;}
				state=0;
				break;
				}//end case 5
			case 4:{
				//Graceful exit.
				flagReceiveBone2=fFalse;
				for (int i=0; i<strLoc; i++){recString[i]=NULL;}
				state=0;
				break;
				}//end case 6						
			default:{flagReceiveBone2=fFalse; state=0;break;}
			}//end switch
	}//end while(flagUARTbone)	
}//end ReceiveBone()
/*************************************************************************************************************/
void ReceiveBone(){
	char recChar, recString[40];
	BYTE state=0, strLoc=0;
	BOOL noCarriage=fTrue;

	/*****************************************State Machine Explained************************************************/
	/* State 0: Got a UART_RX interrupt, take the character and store it in a string if not '.'. If so, go to 6   	*/
	/* State 1: Receive the entire string. if overflow, go to error, otherwise wait for '.' or '\0', then go to 2	*/
	/* State 2: See what was sent to us. If starts with T, getting Trip data, go to 3. if a 'D.', done sending trip */
	/* 	    data and we should exit.																				*/ 	
	/* State 3: Store the trip data in a <char *> vector. Reset receive string then go back to receive. If we		*/
	/* 	    already have trips we need to get rid of the others, must hav ebeen an error receiving. Go to 7,		*/
	/*	    then come back and laod the trips into the vector.														*/ 
	/* State 4: Exit case.																							*/
	/* State 5: Error in ACK case. Send "E." then exit.																*/
	/* State 6: Clear out the USBtripsViewer vector; either USB eject or error in comm to GAVR from Bone w/ trips.	*/
	/****************************************************************************************************************/

	while (flagReceiveBone){
		switch(state){
			case 0:{
				//Get the first character. If a '.', exit to bad state.
				strLoc=0;
				recChar = UDR0;
				if (recChar=='.'){
					state=5;
				} else  {recString[strLoc++]=recChar; state=1;}
				break;
			}//end case 0
			case 1:{
				while (noCarriage && flagReceiveBone){							//while there isn't a timeout and no carry
					while ((!(UCSR0A & (1 << RXC0))) && flagReceiveBone);		//get the next character
					if (!flagReceiveBone){break;}								//if there was a timeout, break out and reset state
					recChar=UDR0;
					recString[strLoc++]=recChar;
					if (recChar == '.' || recChar=='\0'){recString[strLoc]='\0'; noCarriage=fFalse; state=2;}
					else if (strLoc >= 39){state=5;noCarriage=fFalse;}
					else; //end if-elseif-else
				}//end while
				break;
				}//end case 1
			case 2:{
				if (recString[0]=='T'){state=3;}					//getting a trip, store the data.
				else if (!strncmp(recString,"D.",2)){PrintBone2("Got all trips.");flagHaveUSBTrips=fTrue;state=4;}					//exit
				else if (!strncmp(recString,"E.",2)){flagHaveUSBTrips=fFalse;state=4;}
				else {state=5;}									//Error case
				break;
				}//end case 2
			case 3:{
				PrintBone2("Getting trips...");
				//Store trip data in something. Need to add error check on whether we already loaded the trip data.
				if (!flagHaveUSBTrips ){
					PrintBone2("Got ");
					PrintBone2(recString);/*
					char *mallstring=strdup(recString);
					PrintBone2(mallstring);
					USBtripsViewer.push_back(mallstring);						//push the current string on the vector*/
					state=1;
					PrintBone("A.");
				} else {state=6;}		//go to state 7 to clear everything out to receive new trips,  error case.
				for (int i=0; i<strLoc; i++){recString[i]=NULL;}
				strLoc=0;
				noCarriage=fTrue;				
				break;
				}//end case 3
			case 4:{
				//Exiting.
				for (int i=0; i<40; i++){recString[i]=NULL;}
				flagReceiveBone=fFalse;
				break;
				}//end case 4
			case 5:{
				//Error Case
				PrintBone2("Error.");
				PrintBone2(recString);
				PrintBone("E.");
				state=4;
				break;
				}//end case 5
			case 6:{
				//clear out the USBtripsViewer vector
				PrintBone2("Clearing trips...");
				for (int i=0; i<USBtripsViewer.size(); i++){
					char *tempString=USBtripsViewer.pop_back();
					PrintBone2("Popped ");
					PrintBone2(tempString);
				}
				flagHaveUSBTrips=fFalse;			//say we don't have USBtrips, go back to state 3 to push first one on vector
				state=3;
				break;
				}//end case 6				
			default:{flagReceiveBone=fFalse;break;}
		}//end switch
	}//end while(flagReceiveBone)
}//end function
/*************************************************************************************************************/
void SendBone(BYTE whichTrip){
	char recChar, recString[20], checkString[2], sendString[20];
	char *prefaceChar[9]={"T","SP","HR","DI","SD","SY","ME","DE","YE"};						//Speed, HeartRate, Distance, Start Day, Start Year, Minutes Elapsed, Days Elapsed, Years Elapsed
	BYTE sendingWhat=0, state=0, strLoc=0;
	BOOL noCarriage=fTrue;
	WORD offset=0;
	WORD startDays,startYear,minutesElapsed,daysElapsed,yearsElapsed;
	float aveSpeed=0, aveHR=0, distance=0;
	
	//Dependent flags: flagDeleteUSBTrip, flagNeedTrips, flagOffloadTrip
	if (whichTrip != 0 && flagOffloadTrip){
		//Get the trip data we need to send to the beaglebone. We are offloading trip data to the BeagleBone	WORD offset=2+((numberOfTrips-1)*26);
		//Load the trip data from EEPROM based on the whichTrip. If whichTrip=3, need to load the third. AVESPEED for that trip is located at 2+(26*2)
		offset=INITIAL_OFFSET+(whichTrip-1)*BLOCK_SIZE;
		aveSpeed=eeprom_read_float((float*)(offset+AVESPEED));
		aveHR=eeprom_read_float((float*)(offset+AVEHR));
		distance=eeprom_read_float((float*)(offset+DISTANCE));
		startDays=eeprom_read_word((WORD*)(offset+START_DAYS));
		startYear=eeprom_read_word((WORD*)(offset+START_YEAR));
		minutesElapsed=eeprom_read_word((WORD*)(offset+MINUTES_ELAPSED));
		daysElapsed=eeprom_read_word((WORD*)(offset+DAYS_ELAPSED));
		yearsElapsed=eeprom_read_word((WORD*)(offset+YEARS_ELAPSED));
	} else; //We are not offloading anything to the BeagleBone, but we are viewing which trips are on there.
	
	//Raise Send Flag to allow for Timeout to occur and protocol to go forward.
	flagSendBone=fTrue;
	
	/*****************************************State Machine Explained************************************************/
	/* State 0: Raise interrupt to BeagleBone.																		*/
	/* State 1: Put together the return string, should be A. Once '.', got o state 2, otherwise go to Error.		*/
	/* State 2: If we are offloading trip, go to state 3, if deleting state 9, if want trips go to 7. Otherwise 6.  */
	/* State 3: Compare the string. If initial 'A.', go to state 4 as well as if it matches SentString or T.		*/
	/* State 4: Given which trip we have sent, send the next parameter in the trip. Incrmented by sendingWhat and   */
	/*			descriptor is defined by "prefaceChar".																*/
	/* State 5: Done with fields exit.																				*/
	/* State 6: Error case. Print 'E.' then exit.																	*/
	/* State 7: See what came in. If 'A.', tell Bone I want trips in 8.	If 'NT.', exit. Otherwise error.			*/
	/* State 8: Say 'NT.', go to state 1 to put together returning string.											*/
	/* State 9: See what came in. If 'A.', tell bone which trip to delete. If 'Dx' with trip, exit. otherwise error.*/
	/* State 10: Send delete command, go to state 1 to receive acceptance or denial.								*/
	/* State 11: Need to delete GPS, see if it's an A. If so ,go to 12, if what we sent, exit.						*/
	/* State 12: Tell Bone which GPS to delete.																		*/
	/****************************************************************************************************************/	
	
	while (flagSendBone){
		switch(state){	
			case 0:{
				//Raise Interrupts
				prtBONEINT |= (1 << bnBONEINT);
				for (int i=0; i<5; i++){asm volatile("nop");}
				prtBONEINT &= ~(1 << bnBONEINT);
				state=1;
				break;
			}//end case 0
			case 1:{
				//Put together the string that is being received
				while (noCarriage && flagSendBone){
					while ((!(UCSR0A & (1 << RXC0))) && flagSendBone);
					if (!flagSendBone){break;PrintBone2(recString);}	//there was a timeout, don't do anything else. Flags aren't reset.
					recChar=UDR0;
					recString[strLoc++]=recChar;
					if (recChar=='.'){recString[strLoc]='\0'; noCarriage=fFalse; state=2;}			//(4/7 Revision)Forgot to add noCarriage=fFalse;
					else if (strLoc >= 39){strLoc=0; noCarriage=fFalse; state=6;}					//Something went wrong putting together the message/ACK
					else;
				}//end while (noCarriage && flagSendWAVR)
				break;
			}//end case 1			
			case 2:{
				if (flagOffloadTrip){
					state=3;		//offload a trip, multiple acks coming. not worrying about data integrity, only that some information was send (only a few characters anyway.).
				} else if (flagDeleteUSBTrip){	
					state=9;		//tell bone which to delete
				} else if (flagNeedTrips){
					state=7;		//ask for trip data.
				} else if (flagDeleteGAVRTrip){
					state=11;	
				} else if (flagTripOver){
					state=13;						
				}else {state=6;}	//ack errror
				break;
			}//end case 2
			case 3:{
				if (!strncmp(recString,"A.",2)){state=4;}					//Got initial ack, go send data back and forth
				else if (!strcmp(recString,checkString)){state=4;}			//Bone received what it was supposed to.
				else if (!strcmp(recString,"T.")){state=4;}
				else {state=5;}												//ERROR IN ACK case
				break;
			}//end case 3
			case 4:{
				//Go through what we are sending. 
				if (sendingWhat < 9){
					strcpy(checkString,prefaceChar[sendingWhat]);
					PrintBone(prefaceChar[sendingWhat]);
					switch (sendingWhat){
						case 0:{utoa(whichTrip,sendString,10);break;}											//Sends T.
						case 1:{dtostrf(aveSpeed,5,2,sendString);break;}
						case 2:{dtostrf(aveHR,5,2,sendString);break;}
						case 3:{dtostrf(distance,6,2,sendString);break;}		//Distance in miles.
						case 4:{utoa(startDays,sendString,10);break;}
						case 5:{utoa(startYear,sendString,10);break;}
						case 6:{utoa(minutesElapsed,sendString,10);break;}
						case 7:{utoa(daysElapsed,sendString,10);break;}
						case 8:{utoa(yearsElapsed,sendString,10);break;}
						default: {state=6;break;}	
					}
					PrintBone(sendString);
					PutUartChBone('.');					
					sendingWhat++;
					state=1;													//Go back to receive state.
					for (int i=0; i<strLoc; i++){recString[i]=NULL;}
					strLoc=0; 
					noCarriage=fTrue;
				}//end if sendingWhat < 9
				else {state=5;flagOffloadTrip=fFalse;PrintBone("D.");}												//Send D for done.The correct thing what sent, ok to exit
				break;
			}//end case 4				
			case 5:{
				//Exit
				for (int i=0; i<strLoc; i++){recString[i]=NULL;}
				strLoc=0;
				noCarriage=fTrue;
				flagSendBone=fFalse;
				break;
			}
			case 6:{
				//Error
				PrintBone2(recString);
				PrintBone("E.");
				state=5;
				break;
			}
			case 7:{															//Ask for trips.
				if (!strncmp(recString,"A.",2)){state=8;}						//Correct ACK, ask for trips, then exit
				else if (!strncmp(recString,"NT.",3)){state=5;flagNeedTrips=fFalse;}					//Correct ACK from asking for trips, exit
				else {state=6;}													//Go to Error case, then exit.		
				break;
			}
			case 8:{
				PrintBone("NT.");												//Ask Bone to send trips
				state=1;														//Go back and wait for the string to come in
				for (int i=0; i<strLoc; i++){recString[i]=NULL;}
				strLoc=0; 
				noCarriage=fTrue;
				break;
			}
			case 9:{
				if (!strncmp(recString,"A.",2)){state=10;}						//If initial ACK, go tell it which to delete, then exit
				else if (!strcmp(recString,sendString)){state=5;flagDeleteUSBTrip=fFalse;}				//If we got what we sent, go exit without Error.
				else {state=6;}													//If there was an error, exit the correct way.
				break;
			}
			//
			case 10:{
				char tempChar[5];
				utoa(whichTrip,tempChar,10);
				strcpy(sendString,"DU");
				strcat(tempChar,".");
				strcat(sendString,tempChar);
				PrintBone(sendString);											//Should be something like "D4."
				state=1;													//Go back to receive state.
				for (int i=0; i<strLoc; i++){recString[i]=NULL;}
				strLoc=0; 
				noCarriage=fTrue;
				break;
			}
			case 11:{
				if (!strncmp(recString,"A.",2)){state=12;}
				else if (!strcmp(recString,sendString)){state=5; flagDeleteGAVRTrip=fFalse;}
				else {state=6;}
				break;
			}//end case 11
			case 12:{
				char tempChar[5];
				utoa(whichTrip,tempChar,10);
				strcpy(sendString,"DB");
				strcat(tempChar,".");
				strcat(sendString,tempChar);
				PrintBone(sendString);
				state=1;													//Go back to receive state.
				for (int i=0; i<strLoc; i++){recString[i]=NULL;}
				strLoc=0;
				noCarriage=fTrue;
				break;
			}//end case 12
			case 13:{
				if (!strncmp(recString,"A.",2)){state=14;}
				else if (!strncmp(recString,"ST.",3)){state=5; flagTripOver=fFalse;}
				else {state=6;}
				break;
			}//end case 13
			case 14:{
				PrintBone("ST.");
				state=1;													//Go back to receive state.
				for (int i=0; i<strLoc; i++){recString[i]=NULL;}
				strLoc=0;
				noCarriage=fTrue;
				break;
			}//end case 14				
			default: {flagSendBone=fFalse;break;}//default state
		}//end switch
	}//end while(flagSendBone)
}//end SendBone

/*************************************************************************************************************/
//To print to WAVR, variable needs to be false. Print to Bone requires WAVRorBone to be true
void printTimeDate(BOOL WAVRorBone, BOOL pTime,BOOL pDate){
	if (WAVRorBone){ //Printing to BeagleBone
		if (pTime){
			char tempTime[11];
			strcpy(tempTime,currentTime.getTime());
			if (flagReceiveBone2){
				PrintBone2(tempTime);
				PutUartChBone2('/');
			} else {
				PrintBone(tempTime);
				PutUartChBone('/');
			}			
		}
		if (pDate){
			char tempDate[17];
			strcpy(tempDate,currentTime.getDate());
			if (flagReceiveBone2){
				PrintBone2(tempDate);
				PrintBone2(".\0");				
			} else {
				PrintBone(tempDate);
				PrintBone(".\0");
			}			
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
