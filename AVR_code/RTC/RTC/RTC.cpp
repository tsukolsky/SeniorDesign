/*******************************************************************************\
| RTC.cpp
| Author: Todd Sukolsky
| Initial Build: 1/28/2013
| Last Revised: 1/31/13
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
|================================================================================
| *NOTES:
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

//USART/BAUD defines
#define FOSC 20000000		//20 MHz
#define ASY_OSC	32768		//32.768 kHz
#define BAUD 9600
#define MYUBRR FOSC/16/BAUD - 1			//declares baud rate

//Sleep and Run Timing constants
#define SLEEP_TICKS_HIGHV	10				//sleeps for 20 seconds when battery is good to go
#define SLEEP_TICKS_LOWV	12				//sleeps for 60 seconds when battery voltage is low, running on backup.

//ADC and Temp defines
#define LOW_BATT_ADC 300    //change
#define HIGH_TEMP	 12900	//~99 celcius

#define __killPeriphPow() {prtENABLE &= ~((1 << bnGPSen)|(1 << bnGAVRen)|(1 << bnLCDen)); prtBBen &= ~(1 << bnBBen);}
#define __powPeriph() {prtENABLE |= ((1 << bnGPSen)|(1 << bnGAVRen)|(1 << bnLCDen)); prtBBen |= (1 << bnBBen);}

//Forward Declarations
void DeviceInit();
void AppInit(unsigned int ubrr);
void EnableRTCTimer();
void PrintBone(char string[]);
void PrintGAVR(char string[]);
void ReceiveBone();
void PutUartChBone(char ch);
void PutUartChGAVR(char ch);
void Wait_ms(volatile int delay);
void Wait_sec(volatile int sec);
void printTimeDate(BOOL GAVRorBONE = fFalse, BOOL pTime = fFalse, BOOL pDate = fFalse);
void GoToSleep(BOOL shortOrLong);
void TakeADC();
void GetTemp();
void SendTimeDateGAVR(BOOL sTime=fFalse, BOOL sDate=fFalse);

//Global Variables
volatile BOOL flagGoToSleep, flagUARTbone,flagNormalMode,flagUserTime, flagUserDate;
volatile BOOL flagUpdateGAVRTime, flagUpdateGAVRDate, flagSendingGAVR, noTimeout;
volatile BOOL flagNewShutdown, flagShutdown,flagGoodTemp, flagGoodVolts, restart,flagFreshStart;
volatile WORD globalADC=0, globalTemp=0;
//volatile int timeOut=0;
myTime currentTime;  //The clock, MUST BE GLOBAL. In final program, will initiate with NOTHING, then GPS will update on the actual time into beaglebone, beaglebone pings us, then dunzo OR have UART into this as well, then get time and be done.

/*--------------------------Interrupt Service Routines------------------------------------------------------------------------------------*/
ISR(PCINT0_vect){
	
	
}	


ISR(INT2_vect){	//about to get time, get things ready
	if (!flagShutdown){		//If things are off, don't let noise do an interrupt. Shouldn't happen anyways.
		UCSR0B |= (1 << RXCIE0);
		flagGoToSleep=fFalse;	//no sleeping, wait for UART_RX
		flagNormalMode=fFalse;
		//Acknowledge connection, disable INT2_vect
		PrintBone("ACKT");
	}	
}

ISR(TIMER2_OVF_vect){
	volatile static int timeOut = 0;
	volatile static int gavrTimeout=0;
	
	currentTime.addSeconds(1);
	if ((flagUARTbone == fTrue || flagGoToSleep == fFalse) && !flagNewShutdown && !restart){ //if waiting for a character in Receive0() or in main program without sleep
		timeOut++;
		if (timeOut >= 6){
			EIMSK |= (1 << INT2);		//re-enable INT2
			flagUARTbone = fFalse;
			flagGoToSleep = fTrue;
			flagNormalMode=fTrue;
			timeOut = 0;
		}
	} else if (timeOut > 0){
		timeOut = 0;
	} else;
	/* This doesnt work
	if (flagSendingGAVR){gavrTimeout++;}
	if (gavrTimeout>10 && flagSendingGAVR){noTimeout=fFalse; flagSendingGAVR=fFalse;gavrTimeout=0;}
	else; 	*/
}

ISR(USART0_RX_vect){
	UCSR0B &= ~(1 << RXCIE0);
	EIMSK=0x00;
	flagUARTbone=fTrue;
	flagNormalMode=fFalse;
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
	if (flagGoodVolts && flagGoodTemp){__powPeriph();flagFreshStart=fTrue;}
	else {flagNormalMode=fTrue;flagFreshStart=fFalse;}
	//main programming loop
	while(fTrue)
	{				
		//If receiving UART string, go get rest of it.
		if (flagUARTbone){
			EIMSK=0;
			ReceiveBone();
			flagUARTbone = fFalse; 
			//PCIMSK |= (1 << PCINT0);
			EIMSK = (1 << INT2);											//enable INT2 interrupt vector again.
			flagGoToSleep = fTrue;
			flagNormalMode=fTrue;
		}
	
		if (flagUpdateGAVRTime || flagUpdateGAVRDate){
			//kill INT2, updating GAVR
			EIMSK=0x00;
			//SendTimeDateGAVR(flagUpdateGAVRTime,flagUpdateGAVRDate);
			EIMSK = (1 << INT2);
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
				__powPeriph();
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
			EIMSK = 0;
			flagGoToSleep = fTrue;
			flagUARTbone = fFalse;
			saveDateTime_eeprom(fTrue,fTrue);
			
			//Alert BeagleBone and Graphics AVR that powerdown is imminent=> raise SHUTDOWN PINS for 3 clk cycles
			prtBBleds |= (1 << bnBBint);
			prtGAVRleds |= (1 << bnGAVRint);
			if (!flagGoodTemp){
				prtBBleds |= (1 << bnBBtemp);
				prtGAVRleds |= (1 << bnGAVRtemp);
			}
			
			//Five seconds for processing to finish on other chips
			Wait_sec(6);	
			
			prtBBleds &= ~((1 << bnBBint)|(1 << bnBBtemp));
			prtGAVRleds &= ~((1 << bnGAVRint)|(1 << bnGAVRtemp));
			
			//Kill power
			__killPeriphPow();
			flagNewShutdown = fFalse;
		}
		
		//If Restart, broadcast date and time to BeagleBone and other AVR
		if (restart){
			EIMSK = (1 << INT2);	//enable BONE interrupt. Will come out with newest time. Give it 10 seconds to kill
			__powPeriph();
			//Check to see if pins are ready. Use timeout of 10 seconds for pins to come high.
			int waitTime = 0;
			while (waitTime < 3 && restart){waitTime++; Wait_sec(1);}
			flagUpdateGAVRDate=fTrue;
			flagUpdateGAVRTime=fTrue;
			//If we get to here, the flag is not reset or there was a timeout. If timout, goes to sleep and on the next cycle it's awake it will try and 
			//get an updated date and time from the BeagleBone. Always update GAVR.			
		}		
		
			
		//If it's time to go to sleep, go to sleep. INT0 or TIM2_overflow will wake it up.
		if (flagGoToSleep){GoToSleep(flagShutdown);}
					
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
	
	//Disable power to all peripherals
	PRR0 |= (1 << PRTWI)|(1 << PRTIM2)|(1 << PRTIM0)|(1 << PRUSART1)|(1 << PRTIM1)|(1 << PRADC)|(1 << PRSPI);  //Turn EVERYTHING off initially except USART0(UART0)

	//Enable status LEDs
	ddrSLEEPled |= (1 << bnSLEEPled);
	ddrSTATUSled |= (1 << bnSTATUSled);
	prtSLEEPled &= ~(1 << bnSLEEPled);	//turn off initially
	prtSTATUSled |= (1 << bnSTATUSled);	//turn on initially
	
	//Enable BB and GAVR alert pins...outputs, no pull by default.
	ddrBBleds |= (1 << bnBBint)|(1 << bnBBtemp);
	ddrGAVRleds |= (1 << bnGAVRint)|(1 << bnGAVRtemp);
	
	//Enable GAVR interrupt pin, our PB3, it's INT2
	ddrGAVRINT |= (1 << bnGAVRINT);
	prtGAVRINT &=  ~(1 << bnGAVRINT);	//set low at first
	
	//Enable enable signals
	ddrENABLE |= (1 << bnGPSen)|(1 << bnGAVRen)|(1 << bnLCDen);
	ddrBBen |= (1 << bnBBen);
	ddrTEMPen |= (1 << bnTEMPen);
	prtTEMPen |= (1 << bnTEMPen);
	

	
	//Enable INT2. Note* Pin change interrupts will NOT wake AVR from Power-Save mode. Only INT0-2 will.
	EICRA = (1 << ISC21)|(1 << ISC20);			//falling edge of INT2 enables interrupt
	EIMSK = (1 << INT2);			//enable INT2 global interrupt
	
	//Enable SPI for TI temperature
	ddrSpi0 |= (1 << bnMosi0)|(1 << bnSck0)|(1 << bnSS0);	//outputs
	ddrSpi0 &= ~(1 << bnMiso0);
	prtSpi0 |= (1 << bnSS0)|(1 << bnSck0);		//keep SS and SCK high
	prtSpi0 &= ~(1 << bnMosi0);		//keep Miso low
	
	//Init variables
	flagGoToSleep = fTrue;			//changes to fTrue in final implementation
	flagShutdown  = fFalse;
	flagUARTbone = fFalse;
	flagNormalMode=fTrue;
	restart=fFalse;
	flagNewShutdown=fFalse;
	flagSendingGAVR=fFalse;
	noTimeout=fTrue;
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

void PutUartChBone(char ch)
{
	while (! (UCSR0A & (1 << UDRE0)) ) { asm volatile("nop"); }
	UDR0 = ch;
}

/*************************************************************************************************************/
void PrintBone(char string[])
{	
	volatile BYTE i;
	i = 0;

	while (string[i]) {
		PutUartChBone(string[i]);  //send byte		
		i += 1;
	}
}

/*************************************************************************************************************/

void PutUartChGAVR(char ch)
{
while (! (UCSR1A & (1 << UDR1)) ) { asm volatile("nop"); }
UDR1 = ch;
}

/*************************************************************************************************************/
void PrintGAVR(char string[])
{	
	volatile BYTE i;
	i = 0;

	while (string[i]) {
		PutUartChGAVR(string[i]);  //send byte		
		i += 1;
	}
}

/*************************************************************************************************************/
void ReceiveBone(){
	//Declare variables
	BOOL noCarriage = fTrue;
	char recString[20];
	char recChar;
	volatile int strLoc = 0;
	
	recChar = UDR0;
	if (recChar=='.'){
		PutUartChBone(recChar);
		noCarriage=fFalse;
	} else {
		recString[strLoc]=recChar;
		strLoc++;
	}
	while (noCarriage && flagUARTbone){ //flag goes down if a timeout occurs.
		recChar=UDR0; //dump, don't needit. wait for nextone
		while (!(UCSR0A & (1 << RXC0)) && flagUARTbone);
		recChar = UDR0;
		//Put string together. Carriage return is dunzo.
		if (recChar == '.'){
			recString[strLoc] = '\0';
			//See what it's asking for
			if (!strcmp(recString,"date")){printTimeDate(fTrue,fFalse,fTrue);}
			else if (!strcmp(recString,"time")){printTimeDate(fTrue,fTrue,fFalse);}
			else if (!strcmp(recString,"both")){printTimeDate(fTrue,fTrue,fTrue);}
			else if (!strcmp(recString,"save")){saveDateTime_eeprom(fTrue,fFalse);PrintBone(recString);}
			else if (!strcmp(recString,"adc")){char tempChar[7]; itoa(globalADC,tempChar,10); tempChar[6]='\0'; PrintBone(tempChar);}
			else if (!strcmp(recString,"temp")){char tempChar[7]; itoa(globalTemp,tempChar,10); tempChar[6]='\0'; PrintBone(tempChar);}
			else if (!strcmp(recString,"NONE") && (restart || flagFreshStart)){ //If we are starting up again, we need to alert GAVR and clear the flags
				PrintBone("ACKNONE");
				if (flagFreshStart){flagFreshStart=fFalse;}
				if (restart){restart=fFalse;}
				flagUserTime=fTrue;
				flagUserDate=fTrue;
			} else if (recString[2] == ':'){//valid string. Update the time anyways. Comes in every 20 minutes or so...
				cli();
				BOOL success=currentTime.setTime(recString);
				sei();
				if (restart){restart=fFalse;} 
				if (flagFreshStart){flagFreshStart=fFalse;}
				PrintBone("ACK");
				if (success && !restart && !flagFreshStart){saveDateTime_eeprom(fTrue,fFalse); flagUpdateGAVRTime=fTrue; PrintBone(recString);}
				else if (success && !restart && flagFreshStart){saveDateTime_eeprom(fTrue,fFalse); flagUpdateGAVRTime=fTrue; flagUserDate=fTrue; PrintBone(recString);}
				else if (success && restart && !flagFreshStart){saveDateTime_eeprom(fTrue,fFalse); flagUpdateGAVRDate=fTrue; flagUpdateGAVRTime=fTrue; PrintBone(recString);}
				else if (!success && restart){flagUpdateGAVRTime=fTrue; flagUpdateGAVRDate=fTrue; PrintBone("bad");}	//sends eeprom time and date
				else if (!success && flagFreshStart){flagUserTime=fTrue; flagUserDate=fTrue; PrintBone("bad");} //need to get user time and date
				else {PrintBone("bad");}
				//Reset flags for startup
				if (restart){restart=fFalse;}
				if (flagFreshStart){flagFreshStart=fFalse;}		
			} else {
				PrintBone("ACK"); 
				PrintBone(recString);
			}
			noCarriage = fFalse;
		} else {
			recString[strLoc++] = recChar;
			if (strLoc >= 19){strLoc = 0; noCarriage = fFalse; PrintBone("ACKERROR");}
		}	
	}	
}

/*************************************************************************************************************/

void printTimeDate(BOOL GAVRorBONE, BOOL pTime,BOOL pDate){
	if (GAVRorBONE){ //Printing to BeagleBone
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
	
	//Re-enable interrupts
	sei();
	
	//Disable ADC hardware/registers
	ADCSRA = 0;
	ADMUX = 0;
	DIDR0 |= (1 << ADC0D);
	
	//Turn off power
	PRR0 |= (1 << PRADC);
	
	//Do work
	Wait_ms(5);

	flagGoodVolts = (adcReading < LOW_BATT_ADC) ? fFalse : fTrue;
	
	globalADC=adcReading;
}

/*************************************************************************************************************/

void GetTemp(){
	WORD rawTemp = 0;
	
	//Power on temp monitor, let it settle
	//prtTEMPen |= (1 << bnTEMPen);
	PRR0 &= ~(1 << PRSPI);	
	SPCR0 |= (1 << MSTR0)|(1 << SPE0)|(1 << SPR00);			//enables SPI, master, fck/64
	Wait_ms(200);
	//Slave select goes low, sck goes low,  to signal start of transmission
	prtSpi0 &= ~((1 << bnSck0)|(1 << bnSS0));
	
	cli();
	//Write to buffer to start transmission
	SPDR0 = 0x00;
	//Wait for data to be receieved.
	while (!(SPSR0 & (1 << SPIF0)));
	rawTemp = (SPDR0 << 8);
	SPDR0 = 0x00;
	while (!(SPSR0 & (1 << SPIF0)));
	rawTemp |= SPDR0;
	//Set flag to correct value.
	flagGoodTemp = (rawTemp < HIGH_TEMP) ? fTrue : fFalse;
	//re enable interrupts
	sei();
	
	//Bring SS high, clear SPCR0 register and turn power off to SPI and device
	prtSpi0 |= (1 << bnSS0)|(1 << bnSck0);
	SPCR0=0x00;	
	//prtTEMPen &= ~(1 << bnTEMPen);
	PRR0 |= (1 << PRSPI);

	globalTemp=rawTemp;
}
/*************************************************************************************************************/
//10 second timeout for this.
void SendTimeDateGAVR(BOOL sTime, BOOL sDate){
	flagSendingGAVR=fTrue;
	prtSLEEPled |= (1 << bnSLEEPled);
	volatile int state=0;
	BOOL communicating=fTrue;
	noTimeout=fTrue;
	volatile int beginningSecond=currentTime.getSeconds();
	volatile int endingSecond=(10+beginningSecond)%60;
	char sentString[30];	//string that was sent, need it for error checking
	//Make sure we should be sending something
	if (sTime || sDate){communicating=fTrue;}
	else {communicating=fFalse;}
	//Main sending  loop
	while(communicating && noTimeout){
		switch(state){
			case 0:{
				//Send interrupt to GAVR, wait for  ACKW
				prtGAVRINT |= (1 << bnGAVRINT);
				for (volatile int i=0; i<2;i++){asm volatile("nop");}	
				prtGAVRINT &= ~(1 << bnGAVRINT);
				//Wait for ACK now
				state=1;
			} case 1: {
				char recChar, recString[30];
				volatile int strLoc=0;
				BOOL noCarriage=fTrue;
				//We sent an interrupt, need to get an ACKW back now.
				while (noCarriage && noTimeout){
					while (!(UCSR1A & (1 << RXC1)) && noTimeout);
					recChar=UDR1;
					if (recChar=='.'){
						recString[strLoc]='\0';
						noCarriage=fFalse; //get out of loop
						if (!strcmp(recString,"ACKW")){//Good to send time and/or date. send and go to next state.
							state=2;
							if (sTime && sDate){
								printTimeDate(fFalse,fTrue,fTrue);
								PutUartChGAVR('.');
								strcpy(sentString,currentTime.getTime());
								strcat(sentString,"/");
								strcat(sentString,currentTime.getDate());
							} else if (sTime && !sDate){
								printTimeDate(fFalse,fTrue,fFalse);
								PutUartChGAVR('.');
								strcpy(sentString,currentTime.getTime());
								strcat(sentString,"/");
							} else {
								printTimeDate(fFalse,fFalse,fTrue);
								PutUartChGAVR('.');
								strcpy(sentString,currentTime.getDate());
							}													
						} else {state=0;}
					} //endif carriage
					else {
						recString[strLoc++]=recChar;
						if (strLoc >= 30){strLoc=0; noCarriage=fFalse; state=0;}
					} //end normal char else
				} //end receiving part
				break;//end case 1
			} case 2: { //Need to get ACK with the date and/or time.
				char recString[30];
				char recChar;
				volatile int strLoc=0;
				BOOL noCarriage=fTrue;
				char noGoodString[30];
				strcpy(noGoodString,"ACKnogood");
				strcat(noGoodString,sentString);
				//get the ack
				while (noCarriage && noTimeout){
					while (!(UCSR1A & (1 << RXC1)) && noTimeout);
					recChar=UDR1;
					if (recChar=='.'){
						recString[strLoc]='\0';
						noCarriage=fFalse;
						if (!strcmp(recString,sentString)){communicating=fFalse;}
						else if (!strcmp(recString,noGoodString)){state=0;} //retry the send
						else if (!strcmp(recString,"ACKERROR")){communicating=fFalse; flagUpdateGAVRDate=fFalse; flagUpdateGAVRTime=fFalse;} //major error, User is going to get date and time and then send it back to this WAVR												 
						
						else {state=0;}
					} else {
						recString[strLoc++]=recChar;
						if (strLoc > 30){strLoc=0; state=0; noCarriage=fFalse;}
					}
				}
				break;
			} //end case 2
			default: {state=0; break;}
		} //end switch
	if (beginningSecond != endingSecond){noTimeout=fTrue;}
	else {noTimeout=fFalse;}
	}//end communicationg	
	if (noTimeout){
		if (sDate && sTime){flagUpdateGAVRDate=fFalse; flagUpdateGAVRTime=fFalse;}
		else if (sDate && !sTime){flagUpdateGAVRDate=fFalse;}
		else if (sTime && !sDate){flagUpdateGAVRTime=fFalse;}
		else;
	} else;
	prtSLEEPled &= ~(1 << bnSLEEPled);
	flagSendingGAVR=fFalse;			
} //end function


/*--------------------------END-Public Funtions--------------------------------------------------------------------------------*/
