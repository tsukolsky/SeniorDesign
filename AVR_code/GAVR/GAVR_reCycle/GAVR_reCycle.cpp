/*******************************************************************************\
| GAVR_reCycle.cpp
| Author: Todd Sukolsky
| Initial Build: 2/12/2013
| Last Revised: 3/4/13
| Copyright of Todd Sukolsky and Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description: This is the main .cpp for the Graphics AVR for the Bike Computer
|			System. The initial implementation only had a real-time clock and is
|			being built up to final release where it will drive an LCD screen, 
|			measure HR using a touch circuit, measure speed with a reed switch
|			, and keep track of Trips. For more information and code for development
|			modules contact Todd Sukolsky at tsukolsky@gmail.com.
|--------------------------------------------------------------------------------
| Revisions: 2/12- Initial Build. Demoing on ATmega644PA. Final implementation is ATmega2560
|			 2/xx- Integrated RTC, started wtih startup procedures. Tried to get
|				   communication from WAVR working, not quite right. Did get simple
|				   back and forth talking with BeagleBone working.
|			 3/4- Implemented state machine to receive strings from WAVR, next is 
|				  to get communication with Bone working and Trip storage management.
|				  Moved all UART transmissions and receive protocols to an external
|				  file "myUart.h".
|				  
|================================================================================
| *NOTES: (1) This document is stored in the GIT repo @ https://github.com/tsukolsky/SeniorDesign.git/AVR_CODE/GAVR.
|			  Development is also going on in that same repo ../HR_TEST for heart
|			  rate sensing and speed detection using a reed swithc. Trip features
|			  are also being tested in that module.
\*******************************************************************************/

using namespace std;

#include <inttypes.h>
#include <math.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include "stdtypes.h"		//holds standard type definitions
#include "myTime.h"			//myTime class, includes the myDate.h inherently
#include "eepromSaveRead.h"	//includes save and read functions as well as checking function called in ISR
#include "ATmegaXX4PA.h"
#include "myUart.h"

//Defines for Frequency and Baud Rates
#define FOSC 20000000
#define BAUD 9600
#define MYUBRR FOSC/16/BAUD -1	//declares Baud Rate for UART

//Implementations for interrupt resets and sets
#define __disableLevel1INT() PCMSK0 = 0x00; EIMSK=0x00;		//disable any interrupt that might screw up reception. Keep the clock ticks going though
#define __enableLevel1INT() PCMSK0 |= (1 << PCINT0); EIMSK |= (1 << INT2);


void DeviceInit();
void AppInit(unsigned int ubrr);
void EnableRTCTimer();
void Wait_ms(volatile int delay);
void Wait_sec(volatile int delay);
void SendToWAVR(BOOL sTime=fFalse, BOOL sDate=fFalse);
void SendToBone();
void ReceiveBone();

//Flags
BOOL justStarted,flagUARTWAVR, flagUARTBone, flagGetUserDate, flagGetUserTime, flagSendTimeToWAVR, flagSendDateToWAVR;
BOOL flagWaitingForWAVR, flagWaitingForBone;

//Global Variables
volatile int startUpTimeout=0;
myTime currentTime;			//The clock, must be global. Initiated as nothing.

/*--------------------------Interrupt Service Routines------------------------------------------------------------------------------------*/
ISR(PCINT0_vect){	// got a signal from Watchdog that time is about to be sent over.
	volatile static int debounceNumber=0;
	if (PINA & (1 << PCINT0)){	//rising edge on PCINT0
		debounceNumber++;
		if (debounceNumber>0){
			cli();
			flagWaitingForBone=fTrue;
			UCSR0B |= (1 << RXCIE0);
			PCMSK0 &= ~(1 << PCINT0);				//disable all PCINTs, INT2
			EIMSK = 0x00;
			//Wait for UART0 signal now, otherwise do nothing
			PrintBone("ACKB");			//ACK grom GAVR
			sei();
			debounceNumber=0;
		}	
	} else;
}

ISR(INT2_vect){
	volatile static int debounceNumber=0;
	debounceNumber++;
	if(debounceNumber>0){
		cli();
		flagWaitingForWAVR=fTrue;
		UCSR1B |= (1 << RXCIE1);	//enable receiver 1
		PCMSK0=0x00;
		EIMSK=0x00;					//disable all PCINTs
		PrintWAVR("ACKW");			//Ack from GAVR
		sei();
		debounceNumber=0;
	} else;
}

ISR(TIMER2_OVF_vect){
	volatile static int WAVRtimeout=0, BONEtimeout=0;
	pinSTATUSled2 |= (1 << bnSTATUSled2);
	//volatile static int timeOut = 0;
	currentTime.addSeconds(1);
	//Timeout functionality
			
	//If WAVR timout is reached (10 seconds) and one of the flags is still up, reset the timeout, bring flags down and enable interrupt pins again		
	if (WAVRtimeout <=3 && (flagUARTWAVR || flagWaitingForWAVR)){WAVRtimeout++;}
	else if ((WAVRtimeout > 3) && (flagUARTWAVR || flagWaitingForWAVR)){WAVRtimeout=0; flagUARTWAVR=fFalse; flagWaitingForWAVR=fFalse; PCMSK0 |=(1 << PCINT0); EIMSK = (1 << INT2);}
	else if (!flagUARTWAVR && !flagWaitingForWAVR && WAVRtimeout > 0){WAVRtimeout=0;}	//Both flags aren't set, make sure the timeout is 0
	else;
	
	if (BONEtimeout <=3 && (flagUARTBone || flagWaitingForBone)){BONEtimeout++;}	
	else if (BONEtimeout > 3 && (flagUARTBone || flagWaitingForBone)){BONEtimeout=0; flagUARTBone=fFalse; flagWaitingForBone=fFalse; EIMSK = (1 << INT2); PCMSK0 |= (1 << PCINT0);}
	else if (!flagUARTBone && !flagWaitingForBone && BONEtimeout >0){BONEtimeout=0;}
	else;
	
	//If we just started, increment the startUpTimeout value. When it hits 30, logic goes into place for user to enter time and date
	if (justStarted){startUpTimeout++; if (startUpTimeout>35) {justStarted=fFalse;}}//take out that second if inside for final implemenation.
	else if (!justStarted);
	else;
}

//ISR for beaglebone uart input
ISR(USART0_RX_vect){
	flagWaitingForBone=fFalse;
	UCSR0B &= ~(1 << RXCIE0);
	flagUARTBone=fTrue;
}

//ISR for WAVR uart input. 
ISR(USART1_RX_vect){
	flagWaitingForWAVR=fFalse;
	UCSR1B &= ~(1 << RXCIE1);	//clear interrupt. Set UART flag
	flagUARTWAVR=fTrue;
}	
/*--------------------------END-Interrupt Service Routines--------------------------------------------------------------------------------*/
/*--------------------------START-Main Program--------------------------------------------------------------------------------------------*/


int main(void)
{
	DeviceInit();		//Sets all ports and ddrs to something
	AppInit(MYUBRR);	//initializes pins for our setting
	EnableRTCTimer();	//RTC
	getDateTime_eeprom(fTrue,fTrue);	//Get last saved date and time.
	sei();
    while(fTrue)
    {
		if (justStarted){
			//If after 30 seconds we haven't gotten anything, ask for date and then send to WAVR
			if(startUpTimeout > 30){
				flagGetUserTime=fTrue;
				flagGetUserDate=fTrue;
				flagSendDateToWAVR=fTrue;
				flagSendTimeToWAVR=fTrue;
				justStarted=fFalse;
			}
		} else if (!justStarted){
			//Make sure STARTING LED gets turned off
			prtDEBUGleds &= ~(1 << bnSTARTINGled);
		} else {asm volatile("nop");}
		
		//Receiving from WAVR. Either a time string or asking user to set date/time/both.
		if (flagUARTWAVR){
			prtDEBUGleds |= (1 << bnWAVRCOMMled);
			ReceiveWAVR();
			prtDEBUGleds &= ~(1 << bnWAVRCOMMled);
		}
		
		//Receiving from the Bone. Could be a number of things. Needs to implement a state machine.
		if (flagUARTBone){
			prtDEBUGleds |= (1 << bnBONECOMMled);
			ReceiveBone();
			prtDEBUGleds &= ~(1 << bnBONECOMMled);
			flagUARTBone=fFalse;
		}			
		
		//Date is not valid, need to ask user for it
		if (flagGetUserDate || flagGetUserTime){
			if (flagGetUserDate && flagGetUserTime){
				prtDEBUGleds |= (1 << bnUSERDATEled)|(1 << bnUSERTIMEled);
				Wait_sec(3);
				flagGetUserTime=fFalse;
				flagGetUserDate=fFalse;
				//SendToWAVR(fTrue,fTrue);
				prtDEBUGleds &= ~((1 << bnUSERDATEled)|(1 << bnUSERTIMEled));
			} else if (flagGetUserDate && !flagGetUserTime){
				prtDEBUGleds |= (1 << bnUSERDATEled);
				Wait_sec(3);
				flagGetUserDate=fFalse;
				//SendToWAVR(fFalse,fTrue);
				prtDEBUGleds &= ~(1 << bnUSERDATEled);
			} else {
				prtDEBUGleds |= (1 << bnUSERTIMEled);
				//SendToWAVR(fTrue,fFalse);
				Wait_sec(3);
				flagGetUserTime=fFalse;
				prtDEBUGleds &= ~(1 << bnUSERTIMEled);
			}
		}		
    }
}


/*************************************************************************************************************/
void DeviceInit(){
	//Set all ports to input with no pull
	DDRB = 0;
	DDRC = 0;
	DDRD = 0;
	
	PORTB = 0;
	PORTC = 0;
	PORTD = 0;
}
/*************************************************************************************************************/
void AppInit(unsigned int ubrr){
	
	//Set BAUD rate of UART
	UBRR0L = ubrr;   												//set low byte of baud rate for uart0
	UBRR0H = (ubrr >> 8);											//set high byte of baud rate for uart0
	//UCSR0A |= (1 << U2X0);										//set high speed baud clock, in ASYNC mode
	UBRR1L = ubrr;													//set low byte of baud for uart1
	UBRR1H = (ubrr >> 8);											//set high byte of baud for uart1
	//UCSR1A |= (1 << U2X1);										//set high speed baud clock, in ASYNC mode			
	//Enable UART_TX0, UART_RX0, UART_RX1, UART_TX1
	UCSR0B = (1 << TXEN0)|(1 << RXEN0);
	UCSR0C = (1 << UCSZ01)|(1 << UCSZ00);							//Asynchronous; 8 data bits, no parity
	UCSR1B = (1 << TXEN1)|(1 << RXEN1);
	UCSR1C = (1 << UCSZ11)|(1 << UCSZ10);
	//Set interrupt vectors enabled
	//UCSR0B |= (1 << RXCIE0);
	//UCSR1B |= (1 << RXCIE1);
	
	//Enable LEDs
	ddrDEBUGleds |= (1 << bnWAVRCOMMled)|(1 << bnWAVRCOMMled)|(1 << bnSTARTINGled)|(1 << bnUSERDATEled)|(1 << bnUSERTIMEled);
	prtDEBUGleds &= ~((1 << bnWAVRCOMMled)|(1 << bnWAVRCOMMled)|(1 << bnUSERDATEled)|(1 << bnUSERTIMEled));
	prtDEBUGleds|= (1 << bnSTARTINGled);
	
	ddrSTATUSled |= (1 << bnSTATUSled);
	ddrSTATUSled2 |= (1 << bnSTATUSled2); //blinks at 30 Hz
	prtSTATUSled2 |= (1 << bnSTATUSled2);
	prtSTATUSled |= (1 << bnSTATUSled);		//signifies device is ON, goes to WAVR AND BONE
	
	//Disable power to all peripherals
	PRR0 |= (1 << PRTWI)|(1 << PRTIM2)|(1 << PRTIM1)|(1 << PRTIM0)|(1 << PRADC)|(1 << PRSPI);  //Turn EVERYTHING off initially except USART0(UART0)

	//Set up Pin Change interrupts
	PCICR = (1 << PCIE0);			//Enable pin chang einterrupt on PA0
	PCMSK0 |= (1 << PCINT0);			//Enable mask register
	EIMSK = (1 << INT2);
	EICRA = (1 << ISC21)|(1 << ISC20);			//falling edge of INT2 enables interrupt
	
	//Initialize flags
	justStarted=fTrue;
	flagUARTWAVR=fFalse;
	flagUARTBone=fFalse;
	flagGetUserDate=fFalse;
	flagGetUserTime=fFalse;
	flagSendTimeToWAVR=fFalse;
	flagSendDateToWAVR=fFalse;

}
/*************************************************************************************************************/
void EnableRTCTimer(){
	//Asynchronous should be done based on TOSC1 and TOSC2
	//Give power back to Timer2
	PRR0 &= ~(1 << PRTIM2);
	Wait_ms(1);	//give it time to power on
	
	//Set to Asynchronous mode, uses TOSC1/TOSC2 pins
	ASSR |= (1 << AS2);
	
	//Set prescaler, initialize registers
	TCCR2B |= (1 << CS22)|(1 << CS20);	//128 prescaler, should click into overflow every second
	while ((ASSR & ((1 << TCR2BUB)|(1 << TCN2UB))));	//wait for it not to be busy
	TIFR2 = (1 << TOV2);								//Clear any interrupts pending for the timer
	TIMSK2 = (1 << TOIE2);								//Enable overflow on it
	
	//Away we go
}
/*************************************************************************************************************/
void Wait_ms(volatile int delay)
{
	volatile int i;

	while(delay > 0){
		for(i = 0; i < 800; i++){
			asm volatile("nop");
		}
		delay -= 1;
	}
}
/*************************************************************************************************************/
void Wait_sec(volatile int delay){
	volatile int startingTime = currentTime.getSeconds();
	volatile int endingTime= (startingTime+delay)%60;
	while (currentTime.getSeconds() != endingTime){asm volatile ("nop");}
}

/*************************************************************************************************************/
void SendToWAVR(BOOL sTime, BOOL sDate){
	volatile int state=0;
	BOOL done=fFalse, noTimeout=fTrue;
	char sentString[30];
	volatile int beginningSecond=currentTime.getSeconds();
	volatile int endingSecond=(beginningSecond+10)%60;
	while (!done && noTimeout){
		switch(state){
			case 0: {
				//Send interrupt to WAVR
			//	if (sTime || sDate){prtOUTPUTTOWAVR |= (1 << bnNUMBER);Wait_ms(1)}
				if (sTime && sDate){PrintWAVR("SYNTD."); state=1;}
				else if (sTime && !sDate){PrintWAVR("SYNT.");state=3;}
				else if (!sTime && sDate){PrintWAVR("SYND.");state=5;}
				else {done=fTrue; break;}
				//Done transmitting, go to next state which is waiting for ACKT/D/TD
			} case 1: {
				char recChar, recString[7];
				volatile int strLoc=0;
				BOOL noCarriage=fTrue;
				//We send SYNTD to the WAVR, need to receive that without getting a timeout before sending it the date and time.
				while (noCarriage && noTimeout){
					while (!(UCSR1A & (1 << RXC1)) && noTimeout);
					recChar=UDR1;
					if (recChar=='.'){
						recString[strLoc]='\0';
						noCarriage=fFalse; //get out of loop
						if (!strcmp(recString,"ACKTD")){//Good to send time and date, go to next state and send date and time.
							state=2;
							printTimeDate(fFalse,fTrue,fTrue);
							PutUartChWAVR('.');
							strcpy(sentString,currentTime.getTime());
							strcat(sentString,"/");
							strcat(sentString,currentTime.getDate());
						} else {state=0; noCarriage=fFalse; PrintWAVR("SYNERROR.");}
					} //endif carriage
					else {
						recString[strLoc++]=recChar;
						if (strLoc >= 7){strLoc=0; noCarriage=fFalse; state=0; PrintWAVR("SYNERROR.");}
					} //end normal char else
				} //end receiving part
				break;//end case 1
			} case 2: { //Need to get ACK with the date and time.
				char recString[30];
				char recChar;
				volatile int strLoc=0;
				BOOL noCarriage=fTrue;
				//get the ack
				while (noCarriage && noTimeout){
					while (!(UCSR1A & (1 << RXC1)) && noTimeout);
					recChar=UDR1;
					if (recChar=='.'){
						recString[strLoc]='\0';
						noCarriage=fFalse;
						if (!strcmp(recString,sentString)){done=fTrue;}
						else {state=0; PrintWAVR("SYNERROR.");}
					} else {
						recString[strLoc++]=recChar;
						if (strLoc > 30){strLoc=0; state=0; noCarriage=fFalse; PrintWAVR("SYNERROR.");}
					}
				}
				break;
			} case 3: {
				char recString[7];
				char recChar;
				volatile int strLoc=0;
				BOOL noCarriage=fTrue;
				//We send SYNT to the WAVR, need to receive that without getting a timeout before sending it the time.
				while (noCarriage && noTimeout){
					while (!(UCSR1A & (1 << RXC1)) && noTimeout);
					recChar=UDR1;
					if (recChar=='.'){
						recString[strLoc]='\0';
						noCarriage=fFalse; //get out of loop
						if (!strcmp(recString,"ACKT")){//Good to send time and date, go to next state and send date and time.
							state=4;
							printTimeDate(fFalse,fTrue,fFalse);
							strcpy(sentString,currentTime.getTime());
							strcat(sentString,"/");
							PutUartChWAVR('.');	//delimiter
						} else {state=0; noCarriage=fFalse; PrintWAVR("SYNERROR.");}
					} //endif carriage
					else {
						recString[strLoc++]=recChar;
						if (strLoc >= 7){strLoc=0; noCarriage=fFalse; state=0; PrintWAVR("SYNERROR.");}
					} //end normal char else
				} //end receiving part
				break;//end case 1
			} case 4: { //Need to get ACK with the time
				char recString[30];
				char recChar;
				volatile int strLoc=0;
				BOOL noCarriage=fTrue;
				//get the ack
				while (noCarriage && noTimeout){
					while (!(UCSR1A & (1 << RXC1)) && noTimeout);
					recChar=UDR1;
					if (recChar=='.'){
						recString[strLoc]='\0';
						noCarriage=fFalse;
						if (!strcmp(recString,sentString)){done=fTrue;}
						else {state=0; PrintWAVR("SYNERROR.");}
					} else {
						recString[strLoc++]=recChar;
						if (strLoc > 30){strLoc=0; state=0; noCarriage=fFalse; PrintWAVR("SYNERROR.");}
					}
				}
				break;
			} case 5: {
				char recString[7];
				char recChar;
				volatile int strLoc=0;
				BOOL noCarriage=fTrue;
				//We send SYNT to the WAVR, need to receive that without getting a timeout before sending it the date.
				while (noCarriage && noTimeout){
					while (!(UCSR1A & (1 << RXC1)) && noTimeout);
					recChar=UDR1;
					if (recChar=='.'){
						recString[strLoc]='\0';
						noCarriage=fFalse; //get out of loop
						if (!strcmp(recString,"ACKD")){//Good to send time and date, go to next state and send date and time.
							state=6;
							printTimeDate(fFalse,fFalse,fTrue);
							PutUartChWAVR('.');	//delimiter
							strcpy(sentString,currentTime.getDate());
						} else {state=0; noCarriage=fFalse; PrintWAVR("SYNERROR.");}
					} //endif carriage
					else {
						recString[strLoc++]=recChar;
						if (strLoc >= 7){strLoc=0; noCarriage=fFalse; state=0; PrintWAVR("SYNERROR.");}
					} //end normal char else
				} //end receiving part
				break;//end case 1
			} case 6: { //Need to get ACK with the date
				char recString[30];
				char recChar;
				volatile int strLoc=0;
				BOOL noCarriage=fTrue;
				//get the ack
				while (noCarriage && noTimeout){
					while (!(UCSR1A & (1 << RXC1)) && noTimeout);
					recChar=UDR1;
					if (recChar=='.'){
						recString[strLoc]='\0';
						noCarriage=fFalse;
						if (!strcmp(recString,sentString)){done=fTrue;}
						else {state=0; PrintWAVR("SYNERROR.");}
					} else {
						recString[strLoc++]=recChar;
						if (strLoc > 30){strLoc=0; state=0; noCarriage=fFalse; PrintWAVR("SYNERROR.");}
					}
				}
				break;
			} default: {state=0; break;}		
		} //end switch
		if (beginningSecond != endingSecond){noTimeout=fTrue;}
		else {noTimeout=fFalse;}//Timeout occurred, get out of loop and don't set flag that it was successful
	}//end done loop
	if (noTimeout && sTime && !sDate){flagSendTimeToWAVR=fFalse;}
	else if (noTimeout && !sTime && sDate){flagSendDateToWAVR=fFalse;}
	else if (noTimeout && sTime && sDate){flagSendDateToWAVR=fFalse; flagSendTimeToWAVR=fFalse;}
	else {/*do nothing, leave flags as they are. Implement a wait time before we try and update again*/}
} //end function


/*************************************************************************************************************/
void ReceiveBone(){
	//Do something with reception here
	volatile static int numberOfErrors=0; //we have three chances to transmit the time correctly, then we ahve user enter data.
	char recString[10];
	char recChar;
	int strLoc=0;
	BOOL noCarriage=fTrue;
	//Get character from UDR0
	recChar=UDR0;
	if (recChar=='.'){
		PrintBone("R");	//signal resend
		noCarriage=fFalse;
	} else {
		recString[strLoc++]=recChar;
	}
	while (noCarriage && flagUARTBone){
		//Get next character
		while (!(UCSR0A & (1 << RXC0)) && flagUARTBone);
		recChar=UDR0;
		if (recChar=='.'){
			recString[strLoc]='\0';			//Null Terminate
			noCarriage=fFalse;				//gets out of while loop
			if (!strcmp(recString,"time")){printTimeDate(fTrue,fTrue,fFalse);}
			else if (!strcmp(recString,"date")){printTimeDate(fTrue,fFalse,fTrue);}
			else if (!strcmp(recString,"both")){printTimeDate(fTrue,fTrue,fTrue);}
			else {PrintBone("ACK"); PrintBone(recString);}
			numberOfErrors=0;
		} else if (numberOfErrors > 3){PrintBone("ACKERROR"); flagUARTBone=fFalse; numberOfErrors=0;}
		else {
			recString[strLoc] = recChar;
			strLoc++;
			if (strLoc >= 10){strLoc = 0; numberOfErrors++; flagUARTBone=fFalse; PrintBone("ACKR");}
		}
	}			
	char throwAwaychar=UDR0;	//make sure it's clear.
}
/*************************************************************************************************************/