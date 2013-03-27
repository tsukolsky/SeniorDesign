/*******************************************************************************************\
| WAVR.cpp
| Author: Todd Sukolsky
| Initial Build: 1/28/2013
| Last Revised: 3/26/13
| Copyright of Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description: This is the main.cpp file for the RTC to be implemented on the 
|				Re.Cycle final cape that plugs into the beaglebone. It needs to 
|				keep accurate time and date for other modules to take and use.
|				It will be pinged over UART for Time and/or date, and will then
|				respond. Time will be initiated by GPS NMEA string.
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
|================================================================================
| *NOTES: (1)3/26- Currently, if the WAVR is going to update the time, but not the date, on the GAVR it has to 
|			  receive the date first. THis is a problem. If statement needs to be changed and also the SendGAVR()
|			  protocol needs to include the "flagWaitingForReceiveGAVR" in protocol.
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
#include "ATmegaXX4PA.h"	//holds bit definitions
#include "myTime.h"			//myTime class, includes the myDate.h inherently
#include "eepromSaveRead.h"	//includes save and read functions as well as checking function called in ISR
#include "myUart.h"

//USART/BAUD defines
#define FOSC				20000000		//20 MHz
#define ASY_OSC				32768			//32.768 kHz
#define BAUD				9600
#define MYUBRR FOSC/16/BAUD - 1				//declares baud rate


//Declare Time between power on triggers (in ms)
#define POWER_UP_INTERVAL	3000

//Declare timeout value
#define COMM_TIMEOUT_SEC	3

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
	
//Forward Declarations
void DeviceInit();
void AppInit(unsigned int ubrr);
void EnableRTCTimer();
void Wait_ms(volatile int delay);
void Wait_sec(volatile int sec);
void GoToSleep(BOOL shortOrLong);
void TakeADC();
void GetTemp();
void PowerUp(WORD interval);
void PowerDown();

/*********************************************GLOBAL VARIABLES***************************************************/
/****************************************************************************************************************/
WORD globalADC=0, globalTemp=0;
//volatile int timeOut=0;
myTime currentTime;  //The clock, MUST BE GLOBAL. In final program, will initiate with NOTHING, then GPS will update on the actual time into beaglebone, beaglebone pings us, then dunzo OR have UART into this as well, then get time and be done.
/****************************************************************************************************************/


/*********************************************GLOBAL FLAGS*******************************************************/
/****************************************************************************************************************/
/*==============================================================================================================*/
BOOL flagGoToSleep, flagReceivingBone,flagNormalMode,flagReceivingGAVR,flagWaitingForReceiveGAVR;
/* flagGoToSleep: go to sleep on end of while(fTrue) loop														*/
/* flagUARTbone: receiving info from the bone																	*/
/* flagNormalMode: normal mode of operation, take ADC and temp readings											*/
/* flagReceivingWAVR: receiving info from the GAVR																*/
/* flagWaitingForReceiveGAVR: waiting to get the date/time from the GAVR, flagUserDate/flagUserTime is set...	*/
/*								--this flag is set once the SendGAVR() is complete with a need to get date/time */
/*==============================================================================================================*/


/*==============================================================================================================*/
BOOL flagUpdateGAVRTime, flagUpdateGAVRDate, flagSendingGAVR, flagUserDate, flagUserTime, flagInvalidDateTime, flagWaitingToSendGAVR, flagNoGPSTime;
/* flagUpdateGAVRtime: got a new time in from GPS, update GAVR													*/
/* flagUpdateGAVRDate: restart with correct date, send to GAVR	---obsolete since we always send time with date */
/* flagSendingGAVR: currently sending info to GAVR																*/
/* flagUserDate: WAVR doesn't have a good lock on time, tell GAVR to get from user								*/
/* flagUserTime: WAVR doesn't have a good lock on time, tell GAVR to get from user								*/
/* flagInvalidDateTime: the time/date we have currently is wrong/invalid										*/
/* flagWaitingToSendGAVR: Unsuccessful transmission first time, let other things know we are waiting.			*/
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
	if ((PINC & (1 << PCINT17)) && !flagShutdown){
		//Do work, correct interrupt
		UCSR1B |= (1 << RXCIE1);
		flagGoToSleep=fFalse;
		flagNormalMode=fFalse;
		__killCommINT();
		//Acknowledge
		PrintGAVR("ACKG");
	}
}	

//INT2: Getting information from BeagleBone
ISR(INT2_vect){	//about to get time, get things ready
	if (!flagShutdown){		//If things are off, don't let noise do an interrupt. Shouldn't happen anyways.
		UCSR0B |= (1 << RXCIE0);
		flagGoToSleep=fFalse;	//no sleeping, wait for UART_RX
		flagNormalMode=fFalse;
		__killCommINT();
		//Acknowledge connection, disable INT2_vect
		PrintBone("ACKT");
	}	
}

//RTC Timer.
ISR(TIMER2_OVF_vect){
	volatile static int timeOut = 0;
	volatile static int gavrSendTimeout=0, boneReceiveTimeout=0, gavrReceiveTimeout=0;
	
	currentTime.addSeconds(1);
	
	//GAVR Transmission Timeout
	if (flagSendingGAVR && gavrSendTimeout <=COMM_TIMEOUT_SEC){gavrSendTimeout++;}
	else if (flagSendingGAVR && gavrSendTimeout > COMM_TIMEOUT_SEC){flagSendingGAVR=fFalse; gavrSendTimeout=0; __enableCommINT();}
	else if (!flagSendingGAVR && gavrSendTimeout > 0){gavrSendTimeout=0;}
	else;
	
	//BeagleBone Reception Timeout
	if (flagReceivingBone && boneReceiveTimeout <=COMM_TIMEOUT_SEC){boneReceiveTimeout++;}
	else if (flagReceivingBone && boneReceiveTimeout > COMM_TIMEOUT_SEC){flagReceivingBone=fFalse; boneReceiveTimeout=0; __enableCommINT();}
	else if (!flagReceivingBone && boneReceiveTimeout > 0){boneReceiveTimeout=0;}
	else;
	
	//GAVR Reception Timeout
	if (flagReceivingGAVR && gavrReceiveTimeout <= COMM_TIMEOUT_SEC){gavrReceiveTimeout++;}
	else if (flagReceivingGAVR && gavrReceiveTimeout > COMM_TIMEOUT_SEC){flagReceivingGAVR=fFalse; boneReceiveTimeout=0; __enableCommINT();}
	else if (!flagReceivingGAVR && boneReceiveTimeout > 0){boneReceiveTimeout=0;}
	else;

}

//UART Receive from BeagleBone
ISR(USART0_RX_vect){
	UCSR0B &= ~(1 << RXCIE0);
	__killCommINT();				//make sure all interrupts are disabled that could cripple protocol
	flagReceivingBone=fTrue;
}

ISR(USART1_RX_vect){
	UCSR1B &= ~(1 <<RXCIE1);	//disable interrupt
	__killCommINT();
	flagReceivingGAVR=fTrue;
}

/*--------------------------END-Interrupt Service Routines--------------------------------------------------------------------------------*/
/*--------------------------START-Main Program--------------------------------------------------------------------------------------------*/

int main(void)
{
	//Setup
	DeviceInit();
	AppInit(MYUBRR);
	EnableRTCTimer();
	getDateTime_eeprom(fTrue,fTrue);
	sei();
	//Prep/make sure power/temp is good
	GetTemp();
	//flagGoodTemp=fTrue;
	TakeADC();
	if (flagGoodVolts && flagGoodTemp){
		PowerUp(POWER_UP_INTERVAL);
		__enableCommINT();
		flagFreshStart=fTrue;
	}
	else {flagNormalMode=fTrue;flagFreshStart=fFalse;}
		
	//main programming loop
	while(fTrue)
	{				
		//If receiving UART string, go get rest of it.
		if (flagReceivingBone){
			ReceiveBone();
			__enableCommINT();
			if (!flagReceivingGAVR){		//Just in case there was an interrupt IMMEDIATELY after the enabling of Communication interrupts
				flagGoToSleep=fTrue;
				flagNormalMode=fTrue;
			}			
		}
		
		//Receiving Data/Signals from GAVR
		if (flagReceivingGAVR){
			ReceiveGAVR();
			__enableCommINT();
			if (!flagReceivingBone){		//Just in case there was an interrupt IMMEDIATELY after the enabling of Communication interrupts
				flagGoToSleep=fTrue;
				flagNormalMode=fTrue;
			}			
		}
		
	
		//Communication with GAVR. Either updating the date/time on it or asking for date and time. The internal send machine deals with the flags.
		if (flagUpdateGAVRTime || flagUpdateGAVRDate || flagUserDate || flagUserTime && !flagWaitingForReceiveGAVR){
			__killCommINT();
			sendGAVR();
			__enableCommINT();
		}

		//When to save to EEPROM. Saves time on lower half of the hour, saves data and time on lower half-hour of midday.
		if (flagNormalMode){
			if (currentTime.getMinutes()%30 == 0){
				if (currentTime.getHours()%12 == 0){
					saveDateTime_eeprom(fTrue,fTrue);
				} else {
					saveDateTime_eeprom(fTrue,fFalse);
				}	
			}						
		}
		
		//Take ADC reading to check battery level, temp to check board temperature.
		if (flagNormalMode){
			TakeADC();
			GetTemp();
			//If both are good & shutodwn is low, keep it low. If shutdown is high, pull low and enable restart
			if (flagGoodVolts && flagGoodTemp){
				PowerUp(POWER_UP_INTERVAL);
				if( flagShutdown == fTrue){restart = fTrue;}
				flagShutdown = fFalse;
			//If one is bad and shutdown is low, pull high as well as pull new shutdown high to indicate imminent power kill
			} else {
				if (flagShutdown == fFalse){
					flagNewShutdown = fTrue;
				}
				flagShutdown = fTrue;
			}
		}			
		
		//About to shutdown, save EEPROM
		if (flagNewShutdown){
			//Make sure nothing messes with the routine that we care about
			__killCommINT();
			flagGoToSleep = fTrue;
			flagReceivingBone = fFalse;
			flagNoGPSTime=fFalse;
			saveDateTime_eeprom(fTrue,fTrue);
			
			//Kill power--Alert comes in that function
			PowerDown();
			flagNewShutdown = fFalse;
		}
		
		//If Restart, broadcast date and time to BeagleBone and other AVR
		if (restart){
			__enableCommINT();	//enable BONE interrupt. Will come out with newest time. Give it 10 seconds to kill
			PowerUp(POWER_UP_INTERVAL);
			//Check to see if pins are ready. Use timeout of 10 seconds for pins to come high.
			int waitTime = 0;
			while (waitTime < 3 && restart){waitTime++; Wait_sec(1);}
			flagUpdateGAVRDate=fTrue;
			flagUpdateGAVRTime=fTrue;
			flagNoGPSTime=fFalse;
			//If we get to here, the flag is not reset or there was a timeout. If timout, goes to sleep and on the next cycle it's awake it will try and 
			//get an updated date and time from the BeagleBone. Always update GAVR.			
		}		
		
		//If it's time to go to sleep, go to sleep. INT0 or TIM2_overflow will wake it up.
		if (flagGoToSleep){GoToSleep(flagShutdown);}
		
		//Add logic for an invalid date and time somehow getting in here
		if (flagInvalidDateTime){
			flagInvalidDateTime=fFalse;
			flagUserTime=fTrue;
			flagUserDate=fTrue;	//ask user to update/confirm both date and time
		}		
					
	}
	
	return 0;
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
	UBRR0H = (ubrr >> 8);
	//UCSR1A |= (1 << U2X1);
	
	//Enable UART_TX1 and UART_RX1
	UCSR1B = (1 << TXEN1)|(1 << RXEN1);
	UCSR1C = (1 << UCSZ11)|(1 << UCSZ10);
	//UCSR1B |= (1 << RXCIE1);
	
	//Disable power to all peripherals
	PRR0 |= (1 << PRTWI)|(1 << PRTIM2)|(1 << PRTIM0)|(1 << PRUSART1)|(1 << PRTIM1)|(1 << PRADC)|(1 << PRSPI);  //Turn EVERYTHING off initially except USART0(UART0)

	//Enable status LEDs
	ddrSLEEPled |= (1 << bnSLEEPled);
	ddrSTATUSled |= (1 << bnSTATUSled);
	prtSLEEPled &= ~(1 << bnSLEEPled);	//turn off initially
	prtSTATUSled |= (1 << bnSTATUSled);	//turn on initially
	
	//Enable BB and GAVR alert pins...outputs, no pull by default.
	ddrBONEINT |= (1 << bnBBint);
	ddrGAVRINT |= (1 << bnGAVRint);
	
	//Enable GAVR interrupt pin, our PB3, it's INT2
	ddrGAVRINT |= (1 << bnGAVRINT);
	prtGAVRINT &=  ~(1 << bnGAVRINT);	//set low at first
	
	//Enable enable signals
	ddrENABLE |= (1 << bnGPSen)|(1 << bnGAVRen)|(1 << bnLCDen)|(1 << bnBBen);
	ddrTEMPen |= (1 << bnTEMPen);
	ddrMAINen |= (1 << bnMAINen);
	PowerDown();
	__killTemp();

	
	//Enable INT2. Note* Pin change interrupts will NOT wake AVR from Power-Save mode. Only INT0-2 will.
	EICRA = (1 << ISC21)|(1 << ISC20);			//falling edge of INT2 enables interrupt
	EIMSK = (1 << INT2);						//enable INT2 global interrupt
	
	//Enable PCINT17
	PCMSK1 |= (1 << PCINT17);
	PCICR |= (1 << PCIE0);
	
	//Enable SPI for TI temperature
	ddrSpi0 |= (1 << bnMosi0)|(1 << bnSck0)|(1 << bnSS0);	//outputs
	ddrSpi0 &= ~(1 << bnMiso0);
	prtSpi0 |= (1 << bnSS0)|(1 << bnSck0);		//keep SS and SCK high
	prtSpi0 &= ~(1 << bnMosi0);		//keep Miso low
	
	//Init variables
	flagGoToSleep = fTrue;			//changes to fTrue in final implementation
	flagReceivingBone = fFalse;
	flagNormalMode=fTrue;

	flagUpdateGAVRTime=fFalse;
	flagUpdateGAVRDate=fFalse;
	flagSendingGAVR=fFalse;
	flagUserTime=fFalse;
	flagUserDate=fFalse;
	flagInvalidDateTime=fFalse;
	flagWaitingToSendGAVR=fFalse;
	flagNoGPSTime=fFalse;
	
	restart=fFalse;
	flagNewShutdown=fFalse;
	flagShutdown  = fFalse;
	flagGoodVolts=fFalse;
	flagGoodTemp=fFalse;
	flagFreshStart=fTrue;
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
void Wait_sec(volatile int sec){
	volatile int startingTime = currentTime.getSeconds();
	volatile int endingTime= (startingTime+sec)%60;
	while (currentTime.getSeconds() != endingTime){asm volatile ("nop");}
}

/*************************************************************************************************************/

void GoToSleep(BOOL shortOrLong){
		sei();
		volatile int sleepTime, sleepTicks = 0;
		//If bool is true, we are in low power mode/backup, sleep for 60 seconds then check ADC again
		if (shortOrLong == fTrue){
			sleepTime = SLEEP_TICKS_LOWV;
			EIMSK = 0;						//no int2
		} else {
			sleepTime = SLEEP_TICKS_HIGHV;
			EIMSK |= (1 << INT2);			//int2 is allowed.
		}
		//Turn off status LED, put on TIM2 led
		prtSTATUSled &= ~(1 << bnSTATUSled);
		prtSLEEPled |= (1 << bnSLEEPled);
		
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
		Wait_ms(10);
		
		//Done sleeping, turn off sleeping led
		prtSLEEPled &= ~(1 << bnSLEEPled);
		prtSTATUSled |= (1 << bnSTATUSled);
}
/*************************************************************************************************************/

void TakeADC(){
	WORD adcReading = 0;
	
	cli();
	//Turn Power on to ADC
	PRR0 &= ~(1 << PRADC);	
	ADMUX |= (1 << REFS1);	//internal 1.1V reference
	ADCSRA |= (1 << ADEN)|(1 << ADPS2);			//clkIO/16
	DIDR0 = 0xFE;								//disable all ADC's except ADC0
	Wait_ms(5);									//Tim for registers to setup
	
	//Run conversion twice, throw first one out
	for (int i = 0; i < 2; i++){ADCSRA |= (1 << ADSC); while (ADCSRA & (1 << ADSC));}	
	
	//Put conversion into buffer
	adcReading = ADCL;
	adcReading |= (ADCH << 8);
		
	//Assign global reading and set flag
	globalADC=adcReading;
	flagGoodVolts = (adcReading < LOW_BATT_ADC) ? fFalse : fTrue;
		
	//Re-enable interrupts
	sei();
	
	//Disable ADC hardware/registers
	ADCSRA = 0;
	ADMUX = 0;
	DIDR0 |= (1 << ADC0D);
	
	//Turn off power
	PRR0 |= (1 << PRADC);
}

/*************************************************************************************************************/

void GetTemp(){
	WORD rawTemp = 0;
	
	//Power on temp monitor, let it settle
	prtTEMPen |= (1 << bnTEMPen);
	PRR0 &= ~(1 << PRSPI);	
	SPCR |= (1 << MSTR)|(1 << SPE)|(1 << SPR0);			//enables SPI, master, fck/64
	Wait_sec(500);
	//Slave select goes low, sck goes low,  to signal start of transmission
	prtSpi0 &= ~((1 << bnSck0)|(1 << bnSS0));
	
	cli();
	//Write to buffer to start transmission
	SPDR = 0x00;
	//Wait for data to be receieved.
	while (!(SPSR & (1 << SPIF)));
	rawTemp = (SPDR0 << 8);
	SPDR = 0x00;
	while (!(SPSR & (1 << SPIF)));
	rawTemp |= SPDR0;
	
	//Set flag to correct value, update global value
	flagGoodTemp = (rawTemp < HIGH_TEMP) ? fTrue : fFalse;
	globalTemp=rawTemp;
	
	//re enable interrupts
	sei();
	
	//Bring SS high, clear SPCR0 register and turn power off to SPI and device
	prtSpi0 |= (1 << bnSS0)|(1 << bnSck0);
	SPCR=0x00;	
	prtTEMPen &= ~(1 << bnTEMPen);
	PRR0 |= (1 << PRSPI);
}
/*************************************************************************************************************/
void PowerUp(WORD interval){
	cli();
	
	//First power on main regulator
	__enableMain();
	Wait_ms(interval);
	
	//Power on BeagleBone next, takes longer time.
	__enableBeagleBone();
	Wait_ms(interval);
	//while (!(pinBBio & (1 << bnW0B9)));	//Wait for GPIO line to go high
	
	//Power on GAVR and Enable GPS
	__enableGPSandGAVR();
	Wait_ms(interval);
	//while (!(pinGAVRio & (1 << bnW3G0)));	//Wait for GPIO line to go high signifying correct boot
	
	//Power on LCD
	__enableLCD();
	Wait_ms(interval);
	sei();
	
}
/*************************************************************************************************************/
void PowerDown(){
	cli();
	//Signify interrupts, wait 6 seconds for all processing to stop.
	prtInterrupts |= (1 << bnBBint)|(1 << bnGAVRint);
	Wait_sec(6);
	prtInterrupts &= ~((1 << bnBBint)|(1 << bnGAVRint));
	__killLCD();
	__killGPSandGAVR();
	
	//Give the BeagleBone another 6 seconds to finish it's stuff, then kill it
	Wait_sec(6);
	__killBeagleBone();
	__killMain();
	sei();
}
/*************************************************************************************************************/
/*--------------------------END-Public Funtions--------------------------------------------------------------------------------*/
