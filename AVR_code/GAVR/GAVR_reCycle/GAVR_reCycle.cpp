/*******************************************************************************\
| GAVR_reCycle.cpp
| Author: Todd Sukolsky
| Initial Build: 2/12/2013
| Last Revised: 3/27/13
| Copyright of Todd Sukolsky and Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description: This is the main .cpp for the Graphics AVR for the Bike Computer
|			System. The initial implementation only had a real-time clock and is
|			being built up to final release where it will drive an LCD screen, 
|			measure HR using a touch circuit, measure speed with a reed switch
|			, and keep track of Trips. For more information and code for development
|			modules contact Todd Sukolsky at tsukolsky@gmail.com.
|--------------------------------------------------------------------------------
| Revisions:2/12- Initial Build. Demoing on ATmega644PA. Final implementation is ATmega2560
|			2/xx- Integrated RTC, started wtih startup procedures. Tried to get
|				  communication from WAVR working, not quite right. Did get simple
|				  back and forth talking with BeagleBone working.
|			3/4-  Implemented state machine to receive strings from WAVR, next is 
|				  to get communication with Bone working and Trip storage management.
|				  Moved all UART transmissions and receive protocols to an external
|				  file "myUart.h".
|			3/14- Checked over GAVR->WAVR transmission in myUart.h, looks good. Just
|				  need to field test now. Small tweaks possibly in main to make code look better.
|			3/27- Started finalizing communication protocols and integration. LCD code
|				  not yet working, so just getting everything ready. Fixed flags to work with WAVR
|				  correctly, made comments for flags and switched small logic for getting the user
|				  date and time (see main program loop). Deleted some functions for receive/send that
|				  were VERY wrong, switcehd to ATmega2560 registers and initializations. All uart
|				  transmission/reception in "myUart.h" now. (2)Added revision 1. from Revisions Needed,
|			      instituted timeout procedure for GAVR->WAVR protocol dependent on flagSendWAVR.
|			      Need to check logic on what flags are raised and the order that things happen; after
|				  check is completed need to institute definate startup procedure with heartrate code
|				  and odometer code (aka TRIP DATA).
|			3/28- Looked over SendWAVR routine and functionality in main, switched logic. Now depends
|				  on two flags to activiate, flagGetWAVRtime and flagSendWAVRtime. The flagSendWAVR
|				  if only used while sending to keep sending and check for a timeout. This allows
|				  "Revisions Needed (1)" to be irrelevant and logic is much easier to usnerstand. Now
|				  working on EEPROM trip logic.
|================================================================================
| Revisions Needed:
|			(1)3/27-- For timeouts on sending procedures (SendWAVR, SendBone), if a timeout
|				  occurs the flag is killed in the thing, so a subsequent send is impossible.
|				  Need to add flags that a teimtout occurred and we should try and send again? 
|				  Could be global and at end of main while check, if high, raise normal flags.
|				  ------->Revision completeted at 10:52PM, 3/27/13
|================================================================================
| *NOTES: (1) This document is stored in the GIT repo @ https://github.com/tsukolsky/SeniorDesign.git/AVR_CODE/GAVR.
|			  Development is also going on in that same repo ../HR_TEST for heart
|			  rate sensing and speed detection using a reed swithc. Trip features
|			  are also being tested in that module.
|		  (2) "Waiting" timeouts can be dealt with in the main program loop, not in interrupt,
|			  since they will be going through them anyways.
|		  (3) Currently, if there is an invalid date&time the GAVR asks the WAVR
|			  for the correct time, not the user. The watchdog should always answer
|			  with the time that it has, then if the user changes it the WAVR iwll
|			  be updated with that time if it is valid. If it's not valid, then we should
|			  ask for the time from the GAVR. 
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
#include "ATmega2560.h"
#include "myUart.h"

/*****************************************/
/**			UART Frequency/BAUD			**/
/*****************************************/
#define FOSC 16000000
#define BAUD 9600
#define MYUBRR FOSC/16/BAUD -1	//declares Baud Rate for UART

/*****************************************/
/**			TIMEOUT DEFINITIONS			**/
/*****************************************/
#define COMM_TIMEOUT_SEC 3
#define STARTUP_TIMEOUT_SEC 40

/*****************************************/
/**			Interrupt Macros			**/
/*****************************************/
#define __killCommINT() EIMSK &= ~((1 << INT0)|(1 << INT1))		//disable any interrupt that might screw up reception. Keep the clock ticks going though
#define __enableCommINT()  EIMSK |= (1 << INT0)|(1 << INT1)
#define __killSpeedINT() EIMSK &= ~(1 << INT6)
#define __enableSpeedINT() EIMSK |= (1 << INT6)

/*****************************************/
/**			Foward Declarations			**/
/*****************************************/
void DeviceInit();
void AppInit(unsigned int ubrr);
void EnableRTCTimer();
void Wait_ms(volatile int delay);
void Wait_sec(volatile int delay);

/*********************************************GLOBAL FLAGS*******************************************************/
/****************************************************************************************************************/
/*==============================================================================================================*/
BOOL justStarted, flagQUIT, flagGetUserClock, flagWAVRtime, flagGetWAVRtime, flagInvalidDateTime, flagUpdateWAVRtime;
/* justStarted: Whether or not the chip just started up. If so, there is a startup timeout and procedure with	*/
/*				flags that goes on to accurately set the Time and Date											*/
/* flagQUIT: We are about to get shut down, need to quick save everything in EEPROM and then just go into		*/
/*			 down mode.																							*/
/* flagWAVRtime: Whether or not we are using time that was sent from the WAVR or time that we got from the user.*/
/* flagGetUserClock: WAVR needs the time (GPS not valid), need to get it from the user							*/
/* flagGetWAVRtime: Get the time from the WAVR, not really sure when this would happen...maybe an error.		*/
/* flagInvalidDateTime: It's been detected that the time/date in the system is invalid...						*/	
/* flagUpdateWAVRtime: We got the User Clock, time to update the WAVR.											*/														
/*==============================================================================================================*/

/*==============================================================================================================*/
BOOL flagReceiveWAVR, flagSendWAVR, flagReceiveBone, flagSendBone, flagWaitingForWAVR, flagWaitingForBone;
/* flagReceiveWAVR: We are receiving, or about to receive, transmission from WAVR								*/
/* flagSendWAVR: Sending to WAVR, or about to. We need to ask the WAVR for date/time or send it the date and	*/
/*				  time it asked for.																			*/
/* flagReceiveBone: Receiving, or about to receive, the transmission from the BeagleBone						*/
/* flagSendBone: Sending to, or about to send, information to the Bone											*/
/* flagWaitingForWAVR: Waiting to receive a UART string from WAVR, let main program know that with this flag.	*/
/* flagWaitingForBone: Waiting to receive a UART string from the BONE, this alerts main program.				*/
/*==============================================================================================================*/


/*****************************************/
/**			Global Variables			**/
/*****************************************/
myTime currentTime;			//The clock, must be global. Initiated as nothing.

/*--------------------------Interrupt Service Routines------------------------------------------------------------------------------------*/
//INT0, getting something from the watchdog
ISR(INT0_vect){	// got a signal from Watchdog that time is about to be sent over.
	cli();
	flagWaitingForWAVR=fTrue;
	UCSR1B |= (1 << RXCIE1);
	__killCommINT();
	//Wait for UART0 signal now, otherwise do nothing
	PrintBone("ACKW");			//ACK grom GAVR
	sei();
}

//INT1, getting something from the BeagleBone
ISR(INT1_vect){
	cli();
	flagWaitingForBone=fTrue;
	UCSR0B |= (1 << RXCIE0);	//enable receiver 1
	__killCommINT();
	PrintWAVR("ACKB");			//Ack from GAVR
	sei();
}

//INT6->IMMININENT SHUTDOWN
ISR(INT6_vect){
	flagQUIT=fTrue;
}

//INT7-> Speed magnet went around.
ISR(INT7_vect){
	//Do something with speed/odometer/trip thing.
}

ISR(TIMER2_OVF_vect){
	volatile static int WAVRtimeout=0, BONEtimeout=0, startupTimeout=0, sendingWAVRtimeout=0;
	//pinSTATUSled2 |= (1 << bnSTATUSled2);
	//volatile static int timeOut = 0;
	currentTime.addSeconds(1);
	//Timeout functionality
			
	//If WAVR receive timout is reached, either waiting or actually sending, reset the timeout, bring flags down and enable interrupt pins again		
	if (WAVRtimeout <= COMM_TIMEOUT_SEC && (flagReceiveWAVR || flagWaitingForWAVR)){WAVRtimeout++;}
	else if ((WAVRtimeout > COMM_TIMEOUT_SEC) && (flagReceiveWAVR || flagWaitingForWAVR)){WAVRtimeout=0; flagReceiveWAVR=fFalse; flagWaitingForWAVR=fFalse; __enableCommINT();}
	else if (!flagReceiveWAVR && !flagWaitingForWAVR && WAVRtimeout > 0){WAVRtimeout=0;}	//Both flags aren't set, make sure the timeout is 0
	else;
	
	if (BONEtimeout <= COMM_TIMEOUT_SEC && (flagReceiveBone || flagWaitingForBone)){BONEtimeout++;}	
	else if (BONEtimeout > COMM_TIMEOUT_SEC && (flagReceiveBone || flagWaitingForBone)){BONEtimeout=0; flagReceiveBone=fFalse; flagWaitingForBone=fFalse; __enableCommINT();}
	else if (!flagReceiveBone && !flagWaitingForBone && BONEtimeout >0){BONEtimeout=0;}
	else;
	
	//If sending to WAVR, institue the timeout
	if (flagSendWAVR && sendingWAVRtimeout <= COMM_TIMEOUT_SEC){sendingWAVRtimeout++;}
	else if (flagSendWAVR && sendingWAVRtimeout > COMM_TIMEOUT_SEC){flagSendWAVR=fFalse; __enableCommINT();}		//this doesn't allow for a resend.
	else if (!flagSendWAVR && sendingWAVRtimeout > 0){sendingWAVRtimeout=0;}
	else;
	
	//If we just started, increment the startUpTimeout value. When it hits 30, logic goes into place for user to enter time and date
	if (justStarted && startupTimeout <= STARTUP_TIMEOUT_SEC){startupTimeout++;}
	else if (justStarted && startupTimeout > STARTUP_TIMEOUT_SEC){
		startupTimeout=0; 
		justStarted=0;
		if (!flagWAVRtime){flagGetWAVRtime=fTrue;}				//if we haven't gotten the date and time from WAVR yet, say that we need it.
		else {flagGetWAVRtime=fFalse;}							//make sure it's low. not really necessary.
	}	
	else if (!justStarted && startupTimeout > 0){startupTimeout=0;}
	else;
	
}

//ISR for beaglebone uart input
ISR(USART0_RX_vect){
	cli();
	flagWaitingForBone=fFalse;
	UCSR0B &= ~(1 << RXCIE0);
	flagReceiveBone=fTrue;
	sei();
}

//ISR for WAVR uart input. 
ISR(USART1_RX_vect){
	cli();
	flagWaitingForWAVR=fFalse;
	UCSR1B &= ~(1 << RXCIE1);	//clear interrupt. Set UART flag
	flagReceiveWAVR=fTrue;
	sei();
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
		//Receiving from WAVR. Either a time string or asking user to set date/time/both.
		if (flagReceiveWAVR && !flagQUIT){
			//prtDEBUGleds |= (1 << bnWAVRCOMMled);
			ReceiveWAVR();
			__enableCommINT();
			//prtDEBUGleds &= ~(1 << bnWAVRCOMMled);
		}
		
		//Receiving from the Bone. Could be a number of things. Needs to implement a state machine.
		if (flagReceiveBone && !flagQUIT){
			//prtDEBUGleds |= (1 << bnBONECOMMled);
			ReceiveBone();
			//prtDEBUGleds &= ~(1 << bnBONECOMMled);
			__enableCommINT();
			flagReceiveBone=fFalse;
		}			
		
		//Send either "I need the date or time" or "Here is the date and time you asked for. If we are still waiting for WAVR, then don't send anything.
		if ((flagGetWAVRtime || flagUpdateWAVRtime) && !flagWaitingForWAVR && !flagQUIT){
			__killCommINT();
			SendWAVR();
			__enableCommINT();
		}
		
		//WAVR hasn't updated the time, need to get it from user...make sure WAVR isn't about to send the time, or waiting to
		if (flagGetUserClock && !flagWaitingForWAVR && !flagQUIT){
			//Get the time and date from the user
			//if time/date valid, set, save,send
			BOOL valid=fFalse;
			if (valid){flagUpdateWAVRtime=fTrue; flagWAVRtime=fFalse; flagGetWAVRtime=fFalse;}				//SendWAVR() will set the flags down accordingly
			else {flagSendWAVR=fFalse;}
		}			
		
		if (flagQUIT){
			//Save all trip date into EEPROM with "unfinished/finished" label.
			
			//Power Down
			SMCR = (1 << SM1);		//power down mode
			SMCR |= (1 << SE);		//enable sleep
			asm volatile("SLEEP");	//go to sleep
		}		
		
		if (flagInvalidDateTime){
			flagGetWAVRtime=fTrue;
		}		
					
    }//end while
}//end main


/*************************************************************************************************************/
void DeviceInit(){
	//Set all ports to input with no pull
	DDRB = 0;
	DDRC = 0;
	DDRD = 0;
	DDRE = 0;
	DDRF = 0;
	DDRG = 0;
	DDRH = 0;
	DDRJ = 0;
	DDRK = 0;
	DDRL = 0;

	PORTB = 0;
	PORTC = 0;
	PORTD = 0;
	PORTE = 0;
	PORTF = 0;
	PORTG = 0;
	PORTH = 0;
	PORTJ = 0;
	PORTK = 0;
	PORTL = 0;
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
	ddrDEBUGled = 0xFF;	//all outputs
	prtDEBUGled = 0x00;	//all low
	ddrDEBUGled2 |= (1 << bnDBG10)|(1 << bnDBG9)|(1 << bnDBG8);
	prtDEBUGled2 &= ~((1 << bnDBG10)|(1 << bnDBG9)|(1 << bnDBG8));
	
	//Disable power to all peripherals
	PRR0 |= (1 << PRTWI)|(1 << PRTIM2)|(1 << PRTIM1)|(1 << PRTIM0)|(1 << PRADC)|(1 << PRSPI);  //Turn EVERYTHING off initially except USART0(UART0)

	//Set up Pin Change interrupts
	EICRA |= (1 << ISC01)|(1 << ISC00)|(1 << ISC11)|(1 << ISC10);			//rising edge of INT0 and INT1 enables interrupt
	EICRB = (1  << ISC61)|(1 << ISC60)|(1 << ISC71)|(1 << ISC70);			//rising edge of INT7 and INT6 enables interrupt
	__killCommINT();
	__killSpeedINT();
	EIMSK |= (1 << INT7);													//let it know if shutdown is imminent
	
	//Initialize flags
	justStarted=fTrue;
	flagReceiveWAVR=fFalse;
	flagReceiveBone=fFalse;
	flagWaitingForWAVR=fFalse;
	flagWaitingForBone=fFalse;
	flagGetUserClock=fFalse;
	flagSendWAVR=fFalse;
	flagSendBone=fFalse;
	flagWAVRtime=fFalse;
	flagQUIT=fFalse;
	flagGetWAVRtime=fFalse;
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
/*************************************************************************************************************/