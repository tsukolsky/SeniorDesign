/*******************************************************************************************\
| WAVR.cpp
| Author: Todd Sukolsky
| Initial Build: 1/28/2013
| Last Revised: 4/18/13
| Copyright of Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description: This is the main.cpp file for the RTC to be implemented on the 
|				Re.Cycle final cape that plugs into the beaglebone. It needs to 
|				keep accurate time and date for other modules to take and use.
|				It will be pinged over UART for Time and/or date, and will then
|				respond. Time will be initiated by GPS NMEA string. See "Revisions"
|				for changes
|--------------------------------------------------------------------------------
| Revisions:1/28- Initial build, bare boned
|			1/29- Added time and date class, main program built
|			1/30- added EEPROM functionality and USART commands on input.
|			1/31- Moving functions around to better fit coding structure/future
|					releases. Changing how UART functions as well, waits for carriage
|					return.			
|			2/2-  Changed functionality of UART to real implementation. Added ADC code
|				  and temp sensing code from TI's chip that can communicate via SPI.
|				  Now the real functionality is in place and has been tested. Clock
|				  is saved every 30 minutes, providing sleep isn't imminent. To ask
|				  for date/time/both INT2 must be raised, then query is accepted with
|				  a timeout of 10 seconds. Device will return to sleep afer. If low
|			      power is sensed from main module, SHUTDOWN procedure goes into affect
|				  to alert other modules. If waking up from RESTART, broadcast of date and 
|				  and time is sent out broadcast on USART0.
|			3/4- Moved UART transmissions/receptions to "myUart.h". See that file
|				 for notes on whats going on in those files. Need to tweak how the 
|				 timeout works.
|		   3/14- Added logic to allow for "flagInvalidDateTime" to work. If that flag gets 
|				 set the main program is responsible for setting flagUserDate and flagUserTime
|				 high to trigger what is going on. See "myUart.h" -> 'Revisions' with this 
|				 same date for more information.
|		   3/26- Integrated PCB pin declarations and started work on GAVR->WAVR. See "myUart.h"
|				 ->'ReceiveGAVR()'. Startup procedures added. UART1 for GAVR added. Renamed
|				 interrupt enables and kills to "__*CommINT()". (2) Tweaked for SPI/644PA, 
|				 slightly differnet SPI register? Might just be the switch from the ATmega328??
|				 See "myUart.h"->ReceiveGAVR(). Need to add overflow case for timer2/ReceiveGAVR
|				 (3)Instituted timeout for RecieveGAVR, added flag "flagWaitingForReceiveGAVR"
|				 which is used to konw when to actually send a "I need something" to the GAVR->
|				 differentiates the sending case and receiving case.
|		   3/27- (1)Changed flagUserDate and flagUserTime to flagUserClock. Replaced flagUpdateGAVR*
|				 with flagUpdateGAVRClock. See "myUart.h". Need to go over start procedure and 
|				 make it solid for GAVR receive and send to work correctly.(2)FInished start procedure,
|				 has timeout and flags are set when 1. GPS sends string, 2. Timeout occurs, 3. restart 
|				 (will send what we had in EEPROM before GPS comes in if it's delayed)
|		   3/31-4/1- Changed clk fused for the clock to run off of 1MHz instead of the 20MHz, @2.7V backup
|				 the chip is unable to boot and access data memory among other things. Had to add some
|				 of the boolean variables to the InitBools() routine (made in an effort to debug.). The GAVR
|				 needs to have it's capacitors killed whenever there is a kill of power, the transient response is quite slow.
|				 In real world application this won't matter though since it will be a long period of time that the device
|				 is powered off. Added a debugging mechanism for turning on and off peripherals, works well. Implements
|				 a counter. Also keeps the temperature monitor on all the time, there is something fishy when you don't,
|				 almost like it is drawing an ubsurd amount of current. 
|			4/2- Added functionality of GPIO line in power up for GAVR. If it doesn't power on when I want, looks for  the GPIO
|				 line and then sends an interrupt.
|			4/3-4/6- Debugging communication protocols between the two AVR chips. Communication from BeagleBone to the WAVR works,
|				 BeagleBone to GAVR works in limited capacity (full functinoality to be added 4/7). See "myUart.h" for mroe 
|				 information/revision history.
|			4/18- Added functinality to check to see whether BeagleBone and GAVR are operating. if they aren't, pulse the POWER for them
|				 and see if they can boot up. Added "Testing" define that allows startup timeout to be eliminated/unneeded. Also added
|				 check of if the BeagleBone and GAVR are up and running. If they aren't when they're supposed to be, goes through a protocol
|				 to reboot them.
|================================================================================
| Revisions Needed: 
|		(1)	  3/26- Currently, if the WAVR is going to update the time, but not the date, on the GAVR it has to 
|			  receive the date first. THis is a problem. If statement needs to be changed and also the SendGAVR()
|			  protocol needs to include the "flagWaitingForReceiveGAVR" in protocol.
|			  --ISSUE RESOLVED: Now if the WAVR needs the date, it needs the time as well since GPS sends 
|				date and time. Without valid GPS, rely on EEPROM (if restart) or else GAVR/USER. If GPS
|				is sending valid data, user can't change date or time, they are "ignorant".
|
|		(2)	  4/18- Need to clear up difference between SPDR and SPDR0 ??? Recognizes both for some reason.
|================================================================================
| Notes:
\*******************************************************************************/
#include <inttypes.h>
#include <math.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include "stdtypes.h"		//holds standard type definitions
#include "ATmegaXX4PA.h"	//holds bit definitions
#include "myTime.h"			//myTime class, includes the myDate.h inherently
#include "eepromSaveRead.h"	//includes save and read functions as well as checking function called in ISR
#include "wavrUart.h"

using namespace std;

//The next define enables/disables the Startup Timeout and therefore never initiates communication with the GAVR unless the BeagleBone sends it a time string. To enable, comment out line.
#define TESTING

//If we are working on the backup board, uncomment this since the processer is a ATmega324PA, not ATmeaga624PA
//#define BACKUP

//USART/BAUD defines
#define FOSC				8000000			//1 MHz
#define ASY_OSC				32768			//32.768 kHz
#define BAUD				9600
#define MYUBRR FOSC/16/BAUD - 1				//declares baud rate

//Declare Time between power on triggers (in ms)
#define POWER_UP_INTERVAL	3

//Declare timeout value
#define COMM_TIMEOUT_SEC	8
#define STARTUP_TIMEOUT_SEC	60

//Sleep and Run Timing constants
#define SLEEP_TICKS_HIGHV	10				//sleeps for 20 seconds when battery is good to go
#define SLEEP_TICKS_LOWV	12				//sleeps for 60 seconds when battery voltage is low, running on backup.

//ADC and Temp defines
#define LOW_BATT_ADC		 882    //change: 882; (7.4*(14.7/114.7)/1.1)(1024)=882
#define HIGH_TEMP			 12900	//~99 celcius

//Power Management Macros
#define __enableGPSandGAVR() prtENABLE |= (1 << bnGPSen)|(1 << bnGAVRen)
#define __killGPSandGAVR()	 prtENABLE &= ~((1 << bnGPSen)|(1 << bnGAVRen))
#define __killBeagleBone()	 prtENABLE &= ~(1 << bnBBen)
#define __enableBeagleBone() prtENABLE |= (1 <<bnBBen)
#define __enableLCD()		 prtENABLE |= (1 << bnLCDen)
#define __killLCD()			 prtENABLE &= ~(1 << bnLCDen)
#define __enableTemp()		 prtTEMPen |= (1 << bnTEMPen)
#define __killTemp()		 prtTEMPen &= ~(1 << bnTEMPen)
#define __enableMain()		 prtMAINen |= (1 << bnMAINen)
#define __killMain()		 prtMAINen &= ~(1 << bnMAINen)

//Interrupt Macros
#define __killCommINT() {EIMSK=0x00; PCMSK2=0x00;}
#define __enableCommINT() {EIMSK|= (1 << INT2); PCMSK2 = (1 << PCINT17);}
#define __killUARTrec()	{UCSR0B |= (1 << RXCIE0); UCSR1B |= (1 << RXCIE1);}	
	
//Forward Declarations
void DeviceInit();
void AppInit(unsigned int ubrr);
void AppInit2(unsigned int ubrr);
void EnableRTCTimer();
void GoToSleep(BOOL shortOrLong);
void TakeADC();
void GetTemp();
void PowerUp(WORD interval);
void PowerDown();
void InitBools();

/*********************************************GLOBAL VARIABLES***************************************************/
/****************************************************************************************************************/
WORD globalADC=0, globalTemp=0;
//volatile int timeOut=0;
myTime currentTime;  //The clock, MUST BE GLOBAL. In final program, will initiate with NOTHING, then GPS will update on the actual time into beaglebone, beaglebone pings us, then dunzo OR have UART into this as well, then get time and be done.
/****************************************************************************************************************/


/*********************************************GLOBAL FLAGS*******************************************************/
/****************************************************************************************************************/
/*==============================================================================================================*/
BOOL flagGoToSleep, flagReceivingBone,flagNormalMode,flagReceivingGAVR,flagWaitingForReceiveGAVR, flagWaitingForSYNGAVR, flagWaitingForSYNBone;
/* flagGoToSleep: go to sleep on end of while(fTrue) loop														*/
/* flagUARTbone: receiving info from the bone																	*/
/* flagNormalMode: normal mode of operation, take ADC and temp readings											*/
/* flagReceivingWAVR: receiving info from the GAVR																*/
/* flagWaitingForReceiveGAVR: waiting to get the date/time from the GAVR, flagUserDate/flagUserTime is set...	*/
/*								--this flag is set once the SendGAVR() is complete with a need to get date/time */
/* flagWaitingForSYNBone:	*/
/* flagWaitingForSYNGAVR:	*/
/*==============================================================================================================*/


/*==============================================================================================================*/
BOOL flagUpdateGAVRClock, flagSendingGAVR, flagUserClock, flagInvalidDateTime, flagWaitingToSendGAVR, flagGPSTime;
/* flagUpdateGAVRClock: Update the GAVR with time and date.														*/
/* flagSendingGAVR: currently sending info to GAVR																*/
/* flagUserDate: WAVR doesn't have a good lock on date and/or time, signifies GAVR providing it.				*/
/* flagInvalidDateTime: the time/date we have currently is wrong/invalid										*/
/* flagWaitingToSendGAVR: Unsuccessful transmission first time, let other things know we are waiting.			*/
/* flagGPSTime: If we are getting valid data from GPS, don't accept time or date from GAVR.						*/
/*==============================================================================================================*/

/*==============================================================================================================*/
BOOL flagNewShutdown, flagShutdown,flagGoodTemp, flagGoodVolts, restart, flagFreshStart;
/* flagNewShutdown: We are about to shut down because somethign is off (temp and/or ADC)						*/
/* flagShutdown: we are in shutdown mode																		*/
/* flagGoodTemp: temperature reading is below the maximum value													*/
/* flagGoodVolts: ADC reading on batt is above the minimum value												*/
/* restart:	we just came out of shutdown, but not a fresh start. Send date and time to GAVR						*/
/* flagFreshStart: brand new starting sequence																	*/
/*==============================================================================================================*/
/****************************************************************************************************************/


/*--------------------------Interrupt Service Routines------------------------------------------------------------------------------------*/
//PCINT_17: Getting information from the GAVR
ISR(PCINT2_vect){
	cli();
	if (PINC & (1 << PCINT17)){
		__killCommINT();
		//Do work, correct interrupt
		UCSR1B |= (1 << RXCIE1);
		flagGoToSleep=fFalse;
		flagNormalMode=fFalse;
		flagWaitingForSYNGAVR=fTrue;
		//Acknowledge
		PrintGAVR("A.");
	}
	sei();
}	
/********************************************************/
//INT2: Getting information from BeagleBone
ISR(INT2_vect){	//about to get time, get things ready
	cli();
	if (!flagShutdown){		//If things are off, don't let noise do an interrupt. Shouldn't happen anyways.
		flagGoToSleep=fFalse;	//no sleeping, wait for UART_RX
		flagNormalMode=fFalse;
		flagWaitingForSYNBone=fTrue;
		__killCommINT();
		//Acknowledge connection, disable INT2_vect
		PrintBone("A.");
		UCSR0B |= (1 << RXCIE0);
	}
	sei();
}
/********************************************************/
//UART Receive from BeagleBone
ISR(USART0_RX_vect){
	cli();
	if (flagWaitingForSYNBone){
		flagReceivingBone=fTrue;
		flagWaitingForSYNBone=fFalse;
	} else {
		flagReceivingBone=fFalse;		//there was a timeout between the initial interrupt and when we are getting this.
	}	
	UCSR0B &= ~(1 << RXCIE0);
	sei();
}
/********************************************************/
ISR(USART1_RX_vect){
	cli();
	if (flagWaitingForSYNGAVR){
		flagReceivingGAVR=fTrue;
		flagWaitingForSYNGAVR=fFalse;
	}else {
		flagReceivingGAVR=fFalse;
	}
	UCSR1B &= ~(1 <<RXCIE1);	//disable interrupt
	sei();
}

/********************************************************/
//RTC Timer.
ISR(TIMER2_OVF_vect){
	cli();
	prtSLEEPled ^= (1 << bnSLEEPled);
	static unsigned int gavrSendTimeout=0, boneReceiveTimeout=0, gavrReceiveTimeout=0, startupTimeout=0;
	
	currentTime.addSeconds(1);
	
	//GAVR Transmission Timeout
	if (flagSendingGAVR && gavrSendTimeout <=COMM_TIMEOUT_SEC){gavrSendTimeout++;}
	else if (flagSendingGAVR && gavrSendTimeout > COMM_TIMEOUT_SEC){__killUARTrec();flagSendingGAVR=fFalse;flagGoToSleep=fTrue; gavrSendTimeout=0; __enableCommINT();}
	else if (!flagSendingGAVR && gavrSendTimeout > 0){gavrSendTimeout=0;}
	else;
	
	//BeagleBone Reception Timeout
	if ((flagReceivingBone|| flagWaitingForSYNBone) && (boneReceiveTimeout <=COMM_TIMEOUT_SEC)){boneReceiveTimeout++;}
	else if ((flagReceivingBone|| flagWaitingForSYNBone) && (boneReceiveTimeout > COMM_TIMEOUT_SEC)){__killUARTrec();flagReceivingBone=fFalse; flagWaitingForSYNBone=fFalse;flagGoToSleep=fTrue; flagNormalMode=fTrue; boneReceiveTimeout=0; __enableCommINT();}
	else if ((!flagReceivingBone && !flagWaitingForSYNBone) && boneReceiveTimeout > 0){boneReceiveTimeout=0;}
	else;

	//GAVR Reception Timeout
	if ((flagReceivingGAVR || flagWaitingForSYNGAVR) && gavrReceiveTimeout <= COMM_TIMEOUT_SEC){gavrReceiveTimeout++;}
	else if ((flagReceivingGAVR || flagWaitingForSYNGAVR) && gavrReceiveTimeout > COMM_TIMEOUT_SEC){__killUARTrec(); flagReceivingGAVR=fFalse;flagGoToSleep=fTrue; flagWaitingForSYNGAVR=fFalse;flagNormalMode=fTrue;gavrReceiveTimeout=0; __enableCommINT();}
	else if ((!flagReceivingGAVR && !flagWaitingForSYNGAVR) && gavrReceiveTimeout > 0){gavrReceiveTimeout=0;}
	else;

#ifndef TESTING	
	//Startup Tiemout for sending clock to GAVR
	if ((flagFreshStart || restart) && startupTimeout <= STARTUP_TIMEOUT_SEC){startupTimeout++;}
	else if ((flagFreshStart || restart) && startupTimeout > STARTUP_TIMEOUT_SEC){
		if (flagFreshStart){flagFreshStart=fFalse; flagUserClock=fTrue; flagUpdateGAVRClock=fFalse;}	//The GPS didn't send valid data, get user clock.
		else if (restart){restart=fFalse; flagUserClock=fFalse; flagUpdateGAVRClock=fTrue;}				//Gps didn't send valid data, we have valid data. This is redundant to the main
		__enableCommINT();																				//--procedure that depends on "if (restart)"
		startupTimeout=0;
		flagGoToSleep=fTrue;
		flagNormalMode=fTrue;
	} else if (!(flagFreshStart || restart) && startupTimeout > 0){startupTimeout=0;}
	else;
	sei();
#endif
}//End timer 2 overflow.


/*--------------------------END-Interrupt Service Routines--------------------------------------------------------------------------------*/
/*--------------------------START-Main Program--------------------------------------------------------------------------------------------*/

int main(void)
{
	//Setup
	DeviceInit();
	AppInit(MYUBRR);
	EnableRTCTimer();
	InitBools();
	getDateTime_eeprom(fTrue,fTrue);
	//Prep/make sure power/temp is good
	Wait_sec(2);
	GetTemp();
	TakeADC();
	flagGoodTemp=fTrue;
	if (flagGoodVolts && flagGoodTemp){				//Good to power on system
		PowerUp(POWER_UP_INTERVAL);
		__enableCommINT();
		flagFreshStart=fTrue;
		flagShutdown=fFalse;
	} else {										//Something isn't right, don't power on the system.
		__killCommINT();
		flagShutdown=fTrue;
		flagFreshStart=fFalse;
	}
	
	prtSTATUSled |= (1 << bnSTATUSled);
	//main programming loop
	while(fTrue)
	{		
		//If receiving UART string, go get rest of it.
		if (flagReceivingBone && !flagReceivingGAVR){
			ReceiveBone();
			Wait_sec(4);
			if (!flagReceivingGAVR){
				__enableCommINT();
				flagGoToSleep=fTrue;
				flagNormalMode=fTrue;
			} //Otherwise, keep the CommInts down...			
		}//end flag Receiving from Bone 
		
		//Receiving Data/Signals from GAVR. GAVR has the priority
		if (flagReceivingGAVR){
			PrintBone("Receiving from WAVR.");
			ReceiveGAVR();
			Wait_sec(4);	
			if (!flagReceivingBone){		
				__enableCommINT();	
				flagGoToSleep=fTrue;
				flagNormalMode=fTrue;
			}
		}//end flag Receiving from GAVR case
		
		//Communication with GAVR. Either updating the date/time on it or asking for date and time. The internal send machine deals with the flags.
		if ((flagUpdateGAVRClock  || flagUserClock) && !flagWaitingForReceiveGAVR && !flagReceivingBone && !flagReceivingGAVR && !flagWaitingForSYNGAVR && !flagWaitingForSYNBone){
			PrintBone("SendingG.");
			sendGAVR();
			Wait_sec(4);
			if (!flagReceivingGAVR && !flagReceivingBone){
				__enableCommINT();
				flagGoToSleep=fTrue;
				flagNormalMode=fTrue;
			}			
		}//end send to GAVR case

		//When to save to EEPROM. Saves time on lower half of the hour, saves data and time on lower half-hour of midday.
		if (flagNormalMode){
			if (currentTime.getMinutes()%30 == 0){
				if (currentTime.getHours()%12 == 0){
					saveDateTime_eeprom(fTrue,fTrue);
				} else {
					saveDateTime_eeprom(fTrue,fFalse);
				}	
			}						
		}//end time capture/save

		//Take ADC reading to check battery level, temp to check board temperature.
		if (flagNormalMode){
			TakeADC();
			GetTemp();
			flagGoodTemp=fTrue;
			//If both are good & shutdown is low, keep it low. If shutdown is high, pull low and enable restart
			if (flagGoodVolts && flagGoodTemp){
				if(flagShutdown){restart = fTrue; flagShutdown=fFalse;}
			//If one is bad and shutdown is low, pull high as well as pull new shutdown high to indicate imminent power kill
			} else {
				if (!flagShutdown){
					flagNewShutdown = fTrue;
					flagShutdown=fTrue;
				}
			}
		}//end normal mode Check Analog Signals		
		
		//About to shutdown, save EEPROM
		if (flagNewShutdown){
			//Make sure nothing messes with the routine that we care about
			__killCommINT();
			flagGoToSleep = fTrue;
			flagReceivingBone = fFalse;
			flagUserClock=fFalse;						//reset this so next boot is correct. Done in restart case as well for redundancy
			saveDateTime_eeprom(fTrue,fTrue);
			
			//Kill power--Alert comes in that function
			PowerDown();
			flagNewShutdown = fFalse;
		}//end new shutdown
		
		//If Restart, broadcast date and time to BeagleBone and other AVR
		if (restart){
			//Enable COmmunication protocols and then power up. Power up specifies the timing for it to happen	
			PowerUp(POWER_UP_INTERVAL);
			__enableCommINT();
			//Update the GAVRClock since it's a restart, we have the correct date and time. If BeagleBone sends GPS data, use that to back it up.
			flagUpdateGAVRClock=fTrue;
			flagUserClock=fFalse;	
			restart=fFalse;	
		}//end restart		
		
		//If it's time to go to sleep, go to sleep. INT0 or TIM2_overflow will wake it up.
		if (flagGoToSleep && (!flagUserClock || !flagUpdateGAVRClock)){GoToSleep(flagShutdown);}
		
		//Add logic for an invalid date and time somehow getting in here
		if (flagInvalidDateTime && !flagShutdown){
			flagInvalidDateTime=fFalse;
			flagUserClock=fTrue;
			flagUpdateGAVRClock=fFalse;
		}//end invalid dateTIme		
				
	#ifndef TESTING			
		//See if Bone and GAVR are awake.
		if (!flagShutdown && flagNormalMode){
			//See if GAVR is aweake and working.
			if (!(pinGAVRio & (1 << bnW3G0))){
				//Watchdog isn't on, try and boot it up. First deliver interrupt
				prtInterrupts |= (1 << bnGAVRint);
				Wait_ms(1);
				prtInterrupts &= ~(1 << bnGAVRint);
				Wait_sec(1);
				if (pinGAVRio & (1 <, bnW3G0)){
					//Try cutting power, then powering back on
					prtENABLE &= ~(1 << bnGAVRen);
					Wait_sec(1);
					prtENABLE |= (1 << bnGAVRen);
				}
			}
			//See if BeagleBone is awake and working
			if (!(pinBBio & (1 << bnW0B9))){
				//Toggle power for this sucker
				__killBeagleBone();
				Wait_sec(2);
				__enableBeagleBone();
				Wait_sec(10);
			}
		}//end if !shutDown
	#endif			
	}//end while forever
}

/*--------------------------END-Main Program-------------------------------------------------------------------------------------*/
/*--------------------------START-Public Funtions--------------------------------------------------------------------------------*/

/*************************************************************************************************************/
void DeviceInit(){
	//Set all ports to input with no pull
	DDRA = 0;
	DDRB = 0;
	DDRC = 0;
	DDRD = 0;
	
	PORTA = 0;
	PORTB = 0;
	PORTC = 0;
	PORTD = 0;
}
/*************************************************************************************************************/
void AppInit(unsigned int ubrr){
	
	//Set BAUD rate of UART
	UBRR0L = ubrr;   												//set low byte of baud rate
	UBRR0H = (ubrr >> 8);											//set high byte of baud rate
	//UCSR0A |= (1 << U2X0);										//set high speed baud clock, in ASYNC mode
	
	//Enable UART_TX0 and UART_RX0
	UCSR0B = (1 << TXEN0)|(1 << RXEN0);
	UCSR0C = (1 << UCSZ01)|(1 << UCSZ00);							//Asynchronous; 8 data bits, no parity
	//UCSR0B |= (1 << RXCIE0);
	
	//Set BAUD for UART1
	UBRR1L = ubrr;
	UBRR1H = (ubrr >> 8);
	//UCSR1A |= (1 << U2X1);
	
	//Enable UART_TX1 and UART_RX1
	UCSR1B = (1 << TXEN1)|(1 << RXEN1);
	UCSR1C = (1 << UCSZ11)|(1 << UCSZ10);
	//UCSR1B |= (1 << RXCIE1);
	__killUARTrec();
	
	//Disable power to all peripherals
	PRR0 |= (1 << PRTWI)|(1 << PRTIM0)|(1 << PRUSART1)|(1 << PRTIM1)|(1 << PRADC)|(1 << PRSPI);  //Turn EVERYTHING off initially except USART0(UART0)

	//Enable status LEDs
	ddrSLEEPled |= (1 << bnSLEEPled);
	ddrSTATUSled |= (1 << bnSTATUSled);
	prtSLEEPled |= (1 << bnSLEEPled);	//turn off initially
	prtSTATUSled &= ~(1 << bnSTATUSled);	//turn on initially
	
	//Enable BB and GAVR interrupts for COMMUNICATION
	ddrBONEINT |= (1 << bnBONEINT);
	ddrGAVRINT |= (1 << bnGAVRINT);
	prtBONEINT &= ~(1 << bnBONEINT);
	prtGAVRINT &= ~(1 << bnGAVRINT);
	
	//Enable GAVR interrupt pin, our PB3, it's INT2
	ddrInterrupts |= (1 << bnGAVRint)|(1 << bnBBint);
	prtInterrupts &= ~((1 << bnGAVRint)|(1 << bnBBint));
	
	//Enable enable signals
	ddrENABLE |= (1 << bnGPSen)|(1 << bnGAVRen)|(1 << bnLCDen)|(1 << bnBBen);
	ddrTEMPen |= (1 << bnTEMPen);
	ddrMAINen |= (1 << bnMAINen);
	__killMain();
	__killBeagleBone();
	__enableTemp();
	__killLCD();
	__killGPSandGAVR();

	//Enable GPIO lines
	ddrBBio |= (1 << bnW1B10)|(1 << bnW2B11);
	prtBBio &= ~((1 << bnW1B10)|(1 << bnW2B11));
	ddrW6B8 |= (1 << bnW6B8);
	prtW6B8 |= (1 << bnW6B8);
	ddrBBio &= ~(1 << bnW0B9);			//Whether BeagleBone is awake or not
	ddrGAVRio |= (1 << bnW4G1)|(1 << bnW5G2);
	prtGAVRio &= ~((1 << bnW4G1)|(1 << bnW5G2));
	ddrGAVRio &= ~(1 << bnW3G0);		//Whether GAVR is running or not
	
	//Enable INT2. Note* Pin change interrupts will NOT wake AVR from Power-Save mode. Only INT0-2 will.
	__killCommINT();
	EICRA = (1 << ISC21)|(1 << ISC20);			//falling edge of INT2 enables interrupt
	//Enable PCINT17
	PCICR |= (1 << PCIE2);
	
	//Enable SPI for TI temperature
	ddrSpi0 |= (1 << bnMosi0)|(1 << bnSck0)|(1 << bnSS0);	//outputs
	ddrSpi0 &= ~(1 << bnMiso0);
	prtSpi0 |= (1 << bnSS0)|(1 << bnSck0);		//keep SS and SCK high
	prtSpi0 &= ~(1 << bnMosi0);		//keep Miso low
	
}
/*************************************************************************************************************/
void InitBools(){
	//Init variables
	flagGoToSleep = fTrue;			//changes to fTrue in final implementation
	flagReceivingBone = fFalse;
	flagNormalMode=fTrue;
	flagReceivingGAVR=fFalse;
	flagWaitingForReceiveGAVR=fFalse;
	flagWaitingForSYNGAVR=fFalse;
	flagWaitingForSYNBone=fFalse;

	flagUpdateGAVRClock=fFalse;
	flagSendingGAVR=fFalse;
	flagUserClock=fFalse;
	flagInvalidDateTime=fFalse;
	flagWaitingToSendGAVR=fFalse;
	flagGPSTime=fFalse;
	
	restart=fFalse;
	//flagNewShutdown=fFalse;
	flagShutdown=fFalse;		//Initialized in startup procedure in beginning of "main"
	flagGoodVolts=fFalse;
	flagGoodTemp=fFalse;
	//flagFreshStart=fTrue;		//Initialized in startup procedure in beginning of "main"
}
/*************************************************************************************************************/
void EnableRTCTimer(){
	//Asynchronous should be done based on TOSC1 and TOSC2
	//Give power back to Timer2
	PRR0 &= ~(1 << PRTIM2);
	Wait_ms(5);	//give it time to power on
	
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

void GoToSleep(BOOL shortOrLong){
		sei();
		int sleepTime, sleepTicks = 0;
		//If bool is true, we are in low power mode/backup, sleep for 60 seconds then check ADC again
		if (shortOrLong){
			sleepTime = SLEEP_TICKS_LOWV;
		} else {
			sleepTime = SLEEP_TICKS_HIGHV;
		}
		//Turn off status LED, put on TIM2 led
		prtSTATUSled &= ~(1 << bnSTATUSled);
		
		//Set to power save, then enable
		SMCR = (1 << SM1)|(1 << SM0);
		SMCR |= (1 << SE);
		
		//Give time to registers
		Wait_ms(1);
		//Go to sleep
		while (sleepTicks < sleepTime && flagGoToSleep){
			asm volatile("SLEEP");
			sleepTicks++;
		} //endwhile
		
		//Give it time to power back on
		Wait_ms(1);
		
		//Done sleeping, turn off sleeping led
		prtSTATUSled |= (1 << bnSTATUSled);
}
/*************************************************************************************************************/

void TakeADC(){
	WORD adcReading = 0;
	
	prtInterrupts |= (1 << bnBBint);
	
	__killCommINT();
	//Turn Power on to ADC
	PRR0 &= ~(1 << PRADC);	
	ADMUX |= (1 << REFS1);	//internal 1.1V reference
	ADCSRA |= (1 << ADEN)|(1 << ADPS2);			//clkIO/16
	DIDR0 = 0xFE;								//disable all ADC's except ADC0
	Wait_ms(2);									//Tim for registers to setup
	
	//cli();
	//Run conversion twice, throw first one out
	for (int i = 0; i < 2; i++){ADCSRA |= (1 << ADSC); while (ADCSRA & (1 << ADSC));}	
	
	//Re-enable interrupts	
	//sei();
	
	//Put conversion into buffer
	adcReading = ADCL;
	adcReading |= (ADCH << 8);
		
	//Assign global reading and set flag
	globalADC=adcReading;
	flagGoodVolts = (adcReading < LOW_BATT_ADC) ? fFalse : fTrue;
		
	//Disable ADC hardware/registers
	ADCSRA = 0;
	ADMUX = 0;
	DIDR0 |= (1 << ADC0D);
	
	//Turn off power
	PRR0 |= (1 << PRADC);

	char tempString[10];
	itoa(globalADC,tempString,10);
	tempString[9]='\0';
	//PrintBone("ADC: ");
	//PrintBone(tempString);
	prtInterrupts &= ~(1 << bnBBint);
	__enableCommINT();
}

/*************************************************************************************************************/

void GetTemp(){
	WORD rawTemp = 0;

	__killCommINT();

	PRR0 &= ~(1 << PRSPI);
		
	#ifdef BACKUP
		SPCR0 |= (1 << MSTR0)|(1 << SPE0)|(1 << SPR00);			//enables SPI, master, fck/64
	#else
		SPCR |= (1 << MSTR)|(1 << SPE)|(1 << SPR0);
	#endif
	
	Wait_sec(2);
	//Slave select goes low, sck goes low,  to signal start of transmission
	prtSpi0 &= ~((1 << bnSck0)|(1 << bnSS0));
	
	cli();
	//Write to buffer to start transmission
	#ifdef BACKUP
		SPDR0 = 0x00;
		//Wait for data to be receieved.
		while (!(SPSR0 & (1 << SPIF0)));
		rawTemp = (SPDR0 << 8);
		SPDR0 = 0x00;
		while (!(SPSR0 & (1 << SPIF0)));
		rawTemp |= SPDR0;
		SPCR0=0x00;	
	#else	
		SPDR=0x00;
		//Wait for data to be receieved.
		while (!(SPSR & (1 << SPIF)));
		rawTemp = (SPDR << 8);
		SPDR= 0x00;
		while (!(SPSR & (1 << SPIF)));
		rawTemp |= SPDR;	
		SPCR=0x00;
	#endif
		
	//Set flag to correct value, update global value
	//flagGoodTemp = (rawTemp < HIGH_TEMP) ? fTrue : fFalse;
	globalTemp=rawTemp;
	
	//re enable interrupts
	sei();
	
	//Bring SS high, clear SPCR0 register and turn power off to SPI and device
	prtSpi0 |= (1 << bnSS0)|(1 << bnSck0);
	//__killTemp();
	PRR0 |= (1 << PRSPI);
	
	char tempString[9];
	itoa(globalTemp,tempString,10);
	tempString[8]='\0';

	__enableCommINT();
}
/*************************************************************************************************************/
void PowerUp(WORD interval){
	__killCommINT();
	
	//First power on main regulator
	__enableMain();
	Wait_sec(interval);
	
	//Power on BeagleBone next, takes longer time.
	__enableBeagleBone();
	Wait_sec(interval*2);
	
	//Power on GAVR and Enable GPS
	__enableGPSandGAVR();

	//Power on LCD
	__enableLCD();
	Wait_sec(interval);
	
	__enableCommINT();
	PRR0 &= ~(1 << PRUSART1);
	
}
/*************************************************************************************************************/
void PowerDown(){
	PRR0 |= (1 << PRUSART1);
	
	__killCommINT();

	//Signify interrupts, wait 6 seconds for all processing to stop.
	prtInterrupts |= (1 << bnGAVRint);
	prtBONEINT |= (1 << bnBONEINT);
	Wait_sec(6);
	prtInterrupts &= ~(1 << bnGAVRint);
	prtBONEINT &= ~(1 << bnBONEINT);						//This signal is used instead of BBint on prtInterrupts since there is an issue with Kernel omap_mux on beaglebone.

	//Kill LCD and GPS/GAVR first.
	__killLCD();
	__killGPSandGAVR();
	
	//Give the BeagleBone another 15 seconds to execute "halt"
	Wait_sec(15);
	__killBeagleBone();
	__killMain();
}
/*************************************************************************************************************/
/*--------------------------END-Public Funtions--------------------------------------------------------------------------------*/
