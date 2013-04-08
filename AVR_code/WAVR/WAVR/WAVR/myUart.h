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
|			 3/4: Added Sending protocol to GAVR, added receive protocol from BeagleBone.
|			3/14: Double checked Bone receive and GAVR send logic. They should now be correct.
|				  Next step is to implements WAVR receive time/date from GAVR. Also need to add 
|				  timing logic in main to allow "Reminder" to GAVR that the watchdog needs the
|				  user date and time if it was asked for. 
|			3/26: Started work on receive protocol from GAVR. (2) Preliminary finish of 
|				  ReceiveGAVR. Need to add timeout to main program TIMER2 overflow.
|				  (3)Finished ReceiveGAVR(). Should be good to go. Just need to test. Added new flag
|				   flagWaitingForReceiveGAVR whcih is triggered after the WAVR asks for the userDate
|				   or userTime.
|			3/27: (1)Changed flag structure so that only one flag for needing date/time from GAVR, one
|				  flag for receiving since the GPS can send the date too. Need to tweak ReceiveBone()
|				  string parsing.(2)Tweaked Bone receive. Got rid of flagNoGPSTime-->inverse case of flagUserClock.
|				  Finished UART transmissions. All that's needed is testing now.
|			3/28: Added one line in SendGAVR()...flagSendingWAVR=fTrue before it starts.
|			4/3-4/6: Major debugging of UART communications. WAVR->BeagleBone works perfectly, no issues.
|				  The transmission to the GAVR from the WAVR works on the SECOND TRY for all transmissions.
|				  The WAVR cannot, however, receive information from the GAVR so if the user enters the clock,
|				  it cannot update this. If this problem is not resolved within 2 hours tomorrow, 4/7, plan is to 
|				  temporarilty scrap it and let the GPS update it given good conditions. Otherwise nothing will
|				  get the new time. Round Robin could be put in place, but that's a big pain in the a**. The issue
|				  with commuincation with GAVR must be timing. On startup timeout the "getUserClock" feature fails
|				  on first send, where the WAVR lines apparently transmit nothing. Then on the second try, the lines
|				  work correctly, however there is a long (1-2 second) delay and then the INT0 (for GAVR) is triggered
|				  and the functionality works perfectly. Tried implementing timing shifts in this module, but that 
|				  failed to make a difference. Prof. Kia is planning to look at it tomorrow, so morning task is to 
|				  write/implement the Trip protocol and BeagleBone UART protocol and BeagleBone Python code; then move on 
|				  to this debugging. (2) Changed messages to shorter lengths, example "ACKBAD." is now "B." and "ACK." is now
|				  "A.". Shortens time needed allowing for cleaner communications.
|================================================================================
| Revisions Needed: (1) OPTIONAL-Add "ACKBAD" case in ReceiveGAVR()
|							--Issue Resolved 4/5
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
extern BOOL flagSendingGAVR, flagUserClock, flagUpdateGAVRClock, flagInvalidDateTime, flagWaitingToSendGAVR;
extern BOOL flagReceivingBone, flagFreshStart, restart, flagReceivingGAVR,flagWaitingForReceiveGAVR;
extern WORD globalADC, globalTemp;
extern myTime currentTime;


/************************************************************************************************************/
void Wait_sec(int sec){
	volatile int startingTime = currentTime.getSeconds();
	volatile int endingTime= (startingTime+sec)%60;
while (currentTime.getSeconds() != endingTime){asm volatile ("nop");}
}
/************************************************************************************************************/
void Wait_ms(int delay)
{
	volatile int i;

	while(delay > 0){
		for(i = 0; i < 200; i++){
			asm volatile("nop");
		}
		delay -= 1;
	}
}
/************************************************************************************************************/
void SendInterruptGAVR(){
	//Raise interrupts to GAVR for three ish clock cycles. Tried waiting....didn't work.
	prtGAVRINT |= (1 << bnGAVRINT);
	for (int i=0; i<2; i++){asm ("nop");}
	prtGAVRINT &= ~(1 << bnGAVRINT);
	Wait_sec(1);
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

void PutUartChGAVR(char ch){
	while (!(UCSR1A & (1 << UDRE1)));
	UDR1=ch;
}
/*************************************************************************************************************/

void PrintGAVR(char string[]){
	BYTE i=0;
	while (string[i]){
		Wait_ms(350);
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
	
	//Set sending flag
	flagSendingGAVR=fTrue;
	
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
				SendInterruptGAVR();
				state=1;
				break;
			}//end case 0
			case 1: {
				Wait_ms(300);		//Wait for connection to set and something to come in.			
				//Put together string that is being received.
				while (noCarriage && flagSendingGAVR){
					Wait_ms(100);				//wait for the register to start to load.
					PrintBone("WaitingW...");
					while (!(UCSR1A & (1 << RXC1)) && flagSendingGAVR);				//wait for next character
					if (!flagSendingGAVR){state=7; break;}		//if timeout is why we broke, just exit
					recChar=UDR1;
					recString[strLoc++]=recChar;
					if (recChar=='.'){recString[strLoc++]='\0'; state=2; noCarriage=fFalse;}
					else if (strLoc >= 39){strLoc = 0; noCarriage=fFalse; state=7;}
					else;
				}//end while
				break;
				}//end case 1
			case 2: {
				if (!strncmp(recString,"A.",2)){state=3;}		//ack
				else if (!strncmp(recString,"G.",2)){state=5; flagWaitingForReceiveGAVR=fTrue;}	//get
				else if (!strncmp(recString,"B.",2)){state=6;}	//BAD
				//send string case.
				else if (flagUpdateGAVRClock && !strcmp(recString,sentString)){PrintBone("Correct back.");state=5;}		//they match, successful send.
				else{PrintBone("Comparing.");PrintBone(recString);state=7;} //invalid ack. ACKERROR goes here.
				break;
				}//end case 2
			case 3:{
				Wait_ms(100);
				if (flagUserClock && !flagUpdateGAVRClock){
					PrintBone("GetTime.");
					PrintGAVR("G.");				
				//If we are updating the gavr, send the time and date together regardless. preface with SYN
				} else if (flagUpdateGAVRClock && !flagUserClock){
					PrintBone("Sending time.");
					strcpy(sentString,"A");			//this is a syn, not ack to save logic in GAVR code. Can change if we want.
					strcat(sentString,currentTime.getTime());
					strcat(sentString,"/");	//add delimiter.
					strcat(sentString,currentTime.getDate());
					strcat(sentString,".\0");
					PutUartChGAVR('S');
					printTimeDate(fFalse,fTrue,fTrue);			//date is terminated by a . so don't need to send character
					Wait_ms(80);
				} else {PrintBone("noflags.");state=7; break;}	//end if-else (what we are doing).
							
				//Reset the recString to receive the next ACK.
				for (int i=0; i<strLoc; i++){recString[i]=NULL;}
					
				//Reset the carriage feature, string location and go back to the receiving state.
				noCarriage=fTrue;
				strLoc=0;
				state=1;
				break;
				}//end case 3
			case 5:{
				//Successful communications overall
				for (int i=0; i<strLoc; i++){recString[i]=NULL;}
				PrintBone("Success.");
				flagSendingGAVR=fFalse;
				flagUpdateGAVRClock=fFalse;
				state=0;
				break;
				}//end case 5
			case 6:{
				//ACKBAD. Check the date and time to see if its okay. IF okay, go to state 7 to set waiting flag. otherwise set invalid time and close through state 5.
				BOOL dateOK = currentTime.checkValidity();
				if (dateOK){state=7;}
				else {state=7; flagInvalidDateTime=fTrue;}
				break;
				}//end case 6
			case 7:{
				//Got the wrong ACK back, or invalid ACK. Wait for next cycle then resend. Keep all the flags the same
				PrintBone("ErrorS.");
				flagSendingGAVR=fFalse;
				for (int i=0; i<strLoc; i++){recString[i]=NULL;}
				state=0;
				break;
				}//end case 7
			default:{state=0; flagSendingGAVR=fFalse; noCarriage=fFalse; break;}
		}//end switch
	}//end while
}//end function 	

/*************************************************************************************************************/
void ReceiveGAVR(){
	unsigned int state=0;
	char recChar, recString[40];
	unsigned int strLoc=0;
	BOOL noCarriage=fTrue;
	
	//While Loop
	while (flagReceivingGAVR){
		/*************************************************************BEGIN STATE MACHINE************************************************/
		/** State 0: Take first character received after ACKG and start string. If '.', exit to state 6, otherwise go to state 2 and   **/
		/**			 collect rest of the string.																					   **/
		/** State 1: Put together the rest of the string, should be a SYN<x>. If there is a '.', go to state 2 which evaluates.	If	   **/
		/**			 there is a string overflow, exit to state 6																	   **/
		/** State 2: See what was just sent to us in the SYN. If SYNNEED, go to state 4. If its a time, check to make sure it's valid  **/
		/**			 so go to state 3, otherwise go to ACKERROR exit case "6".														   **/
		/** State 3: Got a time, check to see if it's valid. Valid? Update and save time; set flag not to listen to GPS until next     **/
		/**			 trip OR restart/shutdown. Not valid, respond with "ACKBAD." then go to exit case 5								   **/
		/** State 4: Got "SYNNEED". If no "flagUser*" is set then it's okay to set the flagUpdateGAVR*" flags. Respond with "ACKNEED", **/
		/**			 go to exit state, otherwise send "ACKNO" and go to exit state 5.												   **/
		/** State 5: Exit case. Lower "flagReceivingGAVR" which causes and exit.													   **/
		/** State 6: ACKERROR state. Send "ACKERROR", then exit through state 5.													   **/
		/** State 7: Successful acquire of time/date.																				   **/
		/** Default: Set state to 0, doesn't really matter though. Exit signalling timeout to sender.								   **/
		/********************************************************************************************************************************/
		switch(state){
				case 0:{
					//Beginning case
					strLoc=0;
					recChar = UDR1;
					if (recChar=='.'){
						state=6;															//Go to error state.
					} else  {recString[strLoc++]=recChar; state=1;}							//Add to string, go to state 2
					break;
				}//end case 0
				case 1:{
					//Assemble string case
					while (noCarriage && flagReceivingGAVR){	//while there isn't a timeout and no carry
						Wait_ms(100);
						while (!(UCSR1A & (1 << RXC1)) && flagReceivingGAVR);				//get the next character
						if (!flagReceivingGAVR){state=0; break;}							//if there was a timeout, break out and reset state
						recChar=UDR1;
						recString[strLoc++]=recChar;										//'.' always included into recString
						if (recChar == '.'){recString[strLoc]='\0'; noCarriage=fFalse; state=2;}
						else if(strLoc >= 39){state=6;noCarriage=fFalse;}
						else;//end if-elseif-else
					}//end while
					break;
				}//end case 1
				case 2:{
					//Got string, see what it is case.
					if (!strncmp(recString,"N.",8)){state=4;} //set appropriate flags and respond in appropriate way.
					else if ((recString[2]==':') != (recString[3]==':')){state=3;PrintBone("Got a time.");}//go parse the string for a time and date. SYN03:33:12/DATE or SYN3:33:12/DATE, either char 4 or 5 is :
					else {state=6;}
					break;
				}//end case 2
				case 3:{
					//Parse for date/time case
					if (flagUserClock){
						//Go through the string and parse for the time. Must go through the time to get the date.
						BOOL successTime=fFalse, successDate=fFalse;			//whether or not we have successfully parsed string
						int counter=1;
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
							PrintGAVR("B.");
							break;
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
							PrintGAVR("B.");
							state=5;
							break;
						}
										
						//Make sure the settings are okay before setting the time. If not, send ACKBAD and exit.
						if (successDate && successTime){
							currentTime.setTime(tempNum[0],tempNum[1],tempNum[2]);
							currentTime.setDate(tempNum1[0],tempNum1[1],tempNum1[2]);
							saveDateTime_eeprom(fTrue,fTrue);
							flagUserClock=fFalse;
							
							PrintBone("Set my time.");							
							//send ACK
							recString[0]='A';
							PrintGAVR(recString);
							state=7;
						} else {
							//Here is a choice: Do we watn  to user to indicate the time is wrong, or just make them set it? Doesn't really matter.
							flagUserClock=fTrue;
							PrintBone("Bad time.");
							state=5;
						}			
						
						//If we wanted date and got it correctly, or wanted time and got it correctly, go to state 7 to ack with the appropriate response
						if (flagUserClock && successDate && successTime){
							flagUserClock=fFalse;
							flagWaitingForReceiveGAVR=fFalse;
							state=7;					//Respond with correct string ACK
						} else {
							PrintGAVR("B.");
							state=5;
						}																										
					} else {	//don't need the date or time, wasn't looking for it. Respond with ACKNO. Should reset all flags on GAVR side.
						PrintGAVR("NO.");
						state=5;
					}					
					// end if-else (flagUserClock)					
					//Exit
					break;
					}//end case 3				
				case 4:{
					Wait_ms(100);
					//Successful SYNNEED case.
					if (!flagUserClock){	//If we don't need the date or time, update with what we have.
						flagUpdateGAVRClock=fTrue;
						PrintGAVR("N.");				//respond with correct ack
						PrintBone("GAVR Needs.");
					} else {
						PrintBone("NO.");
						PrintGAVR("NO.");	//say we can't give you anything, ask the user.
						//Should be expecting something from the GAVR with user date and time, this reminds the GAVR.
					} //end if-else
					state=5;
					break;					
				}//end case 4
				case 5:{
					//Exit case
					for (int i=0; i<strLoc; i++){recString[i]=NULL;}
					flagReceivingGAVR=fFalse;
					flagWaitingForReceiveGAVR=fFalse;
					state=0;		//just in case
					break;
				}//end case 5
				case 6:{
					//Error in ACK case
					Wait_ms(150);
					PrintGAVR("E.");
					PrintBone("ErrorR.");
					state=5;
					break;
				}//end case 6
				case 7:{
					Wait_ms(150);
					//Successful grab of date/time case
					recString[0]='A';
					PrintGAVR(recString);
					state=5;
					PrintBone("Success.");
					break;		
				}//end case 7
				default: {state=0; strLoc=0; flagReceivingGAVR=fFalse; break;}				
			}//end switch	
		}//end while flagReceivingGAVR	
}	
/*************************************************************************************************************/
void ReceiveBone(){
	BYTE state=0,strLoc=0;
	char recChar, recString[40];
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
			/* State 7: Parse the input string for the time and date. Should be time(:)'/'date(,) where : and , are the delimiters. Term by */
			/*			'.'																													*/
			/********************************************************************************************************************************/
			
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
					while (noCarriage && flagReceivingBone){	//while there isn't a timeout and no carry
						Wait_ms(50);
						while ((!(UCSR0A & (1 << RXC0))) && flagReceivingBone);		//get the next character
						if (!flagReceivingBone){break;}					//if there was a timeout, break out and reset state
						recChar=UDR0;
						recString[strLoc++]=recChar;
						if (recChar == '.'){recString[strLoc]='\0'; noCarriage=fFalse; state=2;}
						else if (strLoc >= 39){state=5;noCarriage=fFalse;}
						else; //end if-elseif-else
					}//end while
					break;
					}//end case 1
				case 2:{
					if (!strncmp(recString,"d.",2)){printTimeDate(fTrue,fFalse,fTrue); state=6;}
					else if (!strncmp(recString,"t.",2)){printTimeDate(fTrue,fTrue,fFalse);state=6;}
					else if (!strncmp(recString,"b.",2)){printTimeDate(fTrue,fTrue,fTrue);state=6;}
					else if (!strncmp(recString,"s.",2)){saveDateTime_eeprom(fTrue,fFalse);PrintBone(recString);state=6;}
					else if (!strncmp(recString,"a.",2)){char tempChar[7]; utoa(globalADC,tempChar,10); tempChar[6]='\0'; PrintBone(tempChar);state=6;}
					else if (!strncmp(recString,"te.",3)){char tempChar[7]; utoa(globalTemp,tempChar,10); tempChar[6]='\0'; PrintBone(tempChar);state=6;}
					else if ((recString[2] == ':') != (recString[3] == ':')){//time/date string, go parse in state 7
						state=7;	
					} else if (!strncmp(recString,"N.",2)){PrintBone("N."); state=6;}//NONE time.	
					else {state=5;}						
					break;
					}//end case 2
				case 3:{
					//Successful receive state of time, change SYN to ACK, then print what we got.
					recString[0]='A';
					PrintBone(recString);
					state=0;
					flagReceivingBone=fFalse;
					for (int i=0; i<strLoc; i++){recString[i]=NULL;}
					break;
					}//end case 3
				case 4:{
					//Bad time string.
					PrintBone("B.");
					flagReceivingBone=fFalse;
					for (int i=0; i<strLoc; i++){recString[i]=NULL;}
					state=0;
					break;
					}//end case 4
				case 5:{
					//Didn't get a good ack or there was an error.
					PrintBone("E.");
					PrintBone(recString);
					flagReceivingBone=fFalse;
					for (int i=0; i<strLoc; i++){recString[i]=NULL;}
					state=0;
					break;
					}//end case 5
				case 6:{
					//Graceful exit.
					flagReceivingBone=fFalse;
					for (int i=0; i<strLoc; i++){recString[i]=NULL;}
					state=0;
					break;
					}//end case 6
				case 7:{
					cli();
					//Parse the string
					//Go through the string and parse for the time. Must go through the time to get the date.
					BOOL successTime=fFalse, successDate=fFalse;			//whether or not we have successfully parsed string
					int counter=1;
					int tempNum[3]={0,0,0}, tempNum1[3]={0,0,0},dmy=0, hms=0, placement=0;
					char tempStringNum[7];
					
					//Parse the string for the time. Always looks for the time. If not end of string or '/' indicating start of date, continue
					while (recString[counter] != '/' && recString[counter] != '\0'){
						//If the character isn't a colon, we haven't gotten 3 int values add to tempStringNum
						if (recString[counter]!=':' && hms<3){
							tempStringNum[placement++]=recString[counter];
						//If haven't gotten 3 int's and character is colon, store int(stringNum) into tempNum[<current time param>]
						} else if (hms<2 && recString[counter] == ':') {
							tempNum[hms++] = atoi(tempStringNum);
							for (int j=0; j <= placement; j++){tempStringNum[j]=(char)NULL;}	//reset the string
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
						state=4;
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
						state=4;
					}	
					
					//NOw set the time and date, then save it to EEPROM
					if (successDate && successTime){
						currentTime.setDate(tempNum1[0],tempNum1[1],tempNum1[2]);
						currentTime.setTime(tempNum[0],tempNum[1],tempNum[2]);
						Wait_ms(300);
						saveDateTime_eeprom(fTrue,fTrue);
						//Even if we have updated the date and time with EEPROM data, do it again.
						//flagUpdateGAVRClock=fTrue;
						//Make sure UserCLock flags are down
						flagUserClock=fFalse;
						state=3;	//Graceful exit
						
					} else if (!(successDate && successTime) && restart){
						//Even if we have updated the date and time with EEPROM data, do it again.
						flagUpdateGAVRClock=fTrue;
						flagUserClock=fFalse;
						state=4;	//ACKBAD
					} else if (!(successDate && successTime) && flagFreshStart && !restart){
						flagUserClock=fTrue;
						flagUpdateGAVRClock=fFalse;
						state=4;	//ACKBAD
					} else;		
					
					//Lower restart flags. Should have parallel case in timer just in case this state doesn't happen.
					flagFreshStart=fFalse;
					restart=fFalse;	
					sei();	
					break;
					}//end case 7									
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
