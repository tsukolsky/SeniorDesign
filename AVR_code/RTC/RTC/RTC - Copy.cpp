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
#define SLEEP_TICKS_LOWV	60				//sleeps for 60 seconds when battery voltage is low, running on backup.

//ADC and Temp defines
#define LOW_BATT_ADC 300    //change
#define HIGH_TEMP	 1001023	//change

#define __killPeriphPow() {prtENABLE &= ~((1 << bnGPSen)|(1 << bnGAVRen)|(1 << bnLCDen)); prtBBen &= ~(1 << bnBBen);}
#define __powPeriph() {prtENABLE |= ((1 << bnGPSen)|(1 << bnGAVRen)|(1 << bnLCDen)); prtBBen |= (1 << bnBBen);}

//Forward Declarations
void DeviceInit();
void AppInit(unsigned int ubrr);
void EnableRTCTimer();
void Print0(char string[]);
void Receive0();
void PutUart0Ch(char ch);
void Wait_ms(volatile WORD delay);
void printTimeDate(BOOL pTime = fFalse, BOOL pDate = fFalse);
void GoToSleep(BOOL shortOrLong);
BOOL TakeADC();
BOOL GetTemp();

//Global Variables
volatile BOOL flagGoToSleep, flagUART_RX, flagSecondLevel, flagGettingTime; //*******
volatile BOOL flagNewShutdown, flagShutdown,flagGoodTemp, flagGoodVolts, restart;
myTime currentTime;  //The clock, MUST BE GLOBAL. In final program, will initiate with NOTHING, then GPS will update on the actual time into beaglebone, beaglebone pings us, then dunzo OR have UART into this as well, then get time and be done.

/*--------------------------Interrupt Service Routines------------------------------------------------------------------------------------*/

ISR(INT2_vect){	//about to get asked for date or time, get ready
	if (!flagShutdown){
		flagGoToSleep = fFalse;	//no sleeping, wait for UART_RX
		//Acknowledge connection, disable INT2_vect
		EIMSK &= ~(1 << INT2);			//disable INT2 global interrupt
		Print0("ACKT"); //*******
	}	
}

ISR(TIMER2_OVF_vect){
	volatile static int timeOut = 0;
	currentTime.addSeconds(1);
	if (flagUART_RX == fTrue || flagGoToSleep == fFalse){ //if waiting for a character in Receive0() or in main program without sleep
		timeOut++;
		if (timeOut >= 10){
			EIMSK = (1 << INT2);		//re-enable INT2
			flagUART_RX = fFalse;
			flagGoToSleep = fTrue;
			Print0("TO."); //take this out in final implementation
			timeOut = 0;
		}
	} else if (timeOut > 0){
		timeOut = 0;
	} 	
}

ISR(USART0_RX_vect){
	UCSR0B &= ~(1 << RXCIE0);		//disable receive interrupt vector
	flagUART_RX = fTrue;
	flagSecondLevel = fFalse;
}

/*--------------------------END-Interrupt Service Routines--------------------------------------------------------------------------------*/
/*--------------------------START-Main Program--------------------------------------------------------------------------------------------*/

int main(void)
{
	//Setup
	DeviceInit();
	AppInit(MYUBRR);
	//Prep/make sure power/temp is good
	//GetTemp();
	TakeADC();
	if (flagGoodVolts && flagGoodTemp){__powPeriph();}
	else {restart = fFalse;}
	//Start normal functionality
	EnableRTCTimer();
	getDateTime_eeprom(fTrue,fTrue);			//Load the last saved values in EEPROM
	sei();
	//main programming loop
	while(fTrue)
	{				
		//If receiving UART string, go get rest of it.
		if (flagUART_RX){
			Receive0();
			if (flagGettingTime){beagleReceiveTime();} //********
			flagUART_RX = fFalse;
			UCSR0B |= (1 << RXCIE0);										//enable receive interrupt vector
			EIMSK |= (1 << INT2);											//enable INT2 interrupt vector again.
			flagSecondLevel = fTrue;
			flagGoToSleep = fTrue;
		}
	
		//When to save to EEPROM. Saves time on lower half of the hour, saves data and time on lower half-hour of midday.
		if (flagSecondLevel){
			if (currentTime.getMinutes()%30 == 0){
				if (currentTime.getHours()%12 == 0){
					saveDateTime_eeprom(fTrue,fTrue);
				} else {
					saveDateTime_eeprom(fTrue,fFalse);
				}	
			}						
		}
		
		//Take ADC reading to check battery level, temp to check board temperature.
		if (flagSecondLevel){
			TakeADC();
			//GetTemp();
			//If both are good & shutodwn is low, keep it low. If shutdown is high, pull low and enable restart
			if (flagGoodVolts && flagGoodTemp){
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
		if (flagNewShutdown && flagSecondLevel){
			saveDateTime_eeprom(fTrue,fTrue);
			
			//Alert BeagleBone and Graphics AVR that powerdown is imminent=> raise SHUTDOWN PINS for 3 clk cycles
			prtBBleds |= (1 << bnBBint);
			prtGAVRleds |= (1 << bnGAVRint);
			if (!flagGoodTemp){
				prtBBleds |= (1 << bnBBtemp);
				prtGAVRleds |= (1 << bnGAVRtemp);
			}
			
			//Five seconds for processing to finish on other chips
			Wait_ms(3000);		
			
			prtBBleds &= ~((1 << bnBBint)|(1 << bnBBtemp));
			prtGAVRleds &= ~((1 << bnGAVRint)|(1 << bnGAVRtemp));
			
			//Kill power
			__killPeriphPow();
			flagNewShutdown = fFalse;
		}
		
		//If Restart, broadcast date and time to BeagleBone and other AVR
		if (restart){
			__powPeriph();
			//Check to see if pins are ready. Use timeout of 10 seconds for pins to come high.
			int waitPeriod = 0;
			BOOL noTimeOut = fTrue;
			/*while (!(PINx & ((1 << bnX)|(1 << bnX)) && noTimeOut){
				if (++waitPeriod > 25){noTimeOut = fFalse;}
				Wait_ms(400);
			};*/ 
			if (noTimeOut){
				Wait_ms(5000);			//this gets taken out
				/*alert BB and GAVR that update is coming*/
				Print0("R.");
				Wait_ms(5);
				printTimeDate(fTrue,fTrue);			
				restart = fFalse;
			}
			//If we get to here, the flag is not reset so it will go to sleep and on the next cycle it's awake it will try and 
			//send the tiem and date again, same routine.			
		}		
		
			
		//If it's time to go to sleep, go to sleep. INT0 or TIM2_overflow will wake it up.
		if (flagGoToSleep && flagSecondLevel){GoToSleep(flagShutdown);}
					
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
	UCSR0B |= (1 << RXCIE0);										//enable receive interrupt vector
	
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
	
	//Enable enable signals
	ddrENABLE |= (1 << bnGPSen)|(1 << bnGAVRen)|(1 << bnLCDen);
	ddrBBen |= (1 << bnBBen);
	ddrTEMPen |= (1 << bnTEMPen);
	
	//Enable INT2. Note* Pin change interrupts will NOT wake AVR from Power-Save mode. Only INT0-2 will.
	EICRA = (1 << ISC21);			//falling edge of INT2 enables interrupt
	EIMSK = (1 << INT2);			//enable INT2 global interrupt
	
	//Enable SPI for TI temperature
	ddrSpi0 |= (1 << bnMosi0)|(1 << bnSck0)|(1 << bnSS0);	//outputs
	prtSpi0 |= (1 << bnSS0)|(1 << bnSck0);		//keep SS and SCK high
	prtSpi0 &= ~((1 << bnMiso0)|(1 << bnMosi0));		//keep Miso low
	SPCR |= (1 << MSTR)|(1 << SPE)|(1 << SPR1);			//enables SPI, master, fck/64
	
	//Init variables
	flagGoToSleep = fTrue;			//changes to fTrue in final implementation
	flagShutdown  = fFalse;
	flagUART_RX = fFalse;
	flagSecondLevel = fTrue;
	restart = fTrue;
	
	flagGoodTemp = fTrue;
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
void Wait_ms(volatile WORD delay)
{
	volatile WORD i;

	while(delay > 0){
		for(i = 0; i < 600; i++){
			asm volatile("nop");
		}
		delay -= 1;
	}
}

/*************************************************************************************************************/

void PutUart0Ch(char ch)
{
	while (! (UCSR0A & (1 << UDRE0)) ) { asm volatile("nop"); }
	UDR0 = ch;
}

/*************************************************************************************************************/
void Print0(char string[])
{	
	BYTE i;
	i = 0;

	while (string[i]) {
		PutUart0Ch(string[i]);  //send byte		
		i += 1;
	}
}

/*************************************************************************************************************/
void Receive0(){
	//Declare variables
	volatile BOOL noCarriage = fTrue;
	char recString[20];
	char recChar;
	volatile int strLoc = 0;
	
	recChar = UDR0;	//get initial 
	if (recChar == '.'){
			PutUart0Ch(recChar);
			noCarriage = fFalse;
	} else {
		recString[strLoc] = recChar;
		strLoc++;
	}
	while (noCarriage && flagUART_RX){ //flag goes down if a timeout occurs.
		//Wait to get next character
		while (!(UCSR0A & (1 << RXC0)) && flagUART_RX);
		recChar = UDR0;
		//Put string together. Carriage return is dunzo.
		if (recChar == '.'){
			recString[strLoc] = '\0';
			noCarriage = fFalse;
			//See what it's asking for
			if (!strcmp(recString,"date")){printTimeDate(fFalse,fTrue);}
			else if (!strcmp(recString,"time")){printTimeDate(fTrue,fFalse);}
			else if (!strcmp(recString,"both")){printTimeDate(fTrue,fTrue);}
			else if (!(strcmp(recString,"NONE")){Print0("ACKNONE"); flagGettingTime=fFalse;} //****
			else {Print0(recString);}
			break;
		} else {
			recString[strLoc] = recChar;
			strLoc++;
			if (strLoc >= 19){strLoc = 0; noCarriage = fFalse;}
		}	
	}	
}
/*************************************************************************************************************/
void beagleReceiveTime(){
	volatile BOOL noCarriage = fTrue;
	char recString[10];
	char recChar;
	volatile int strLoc=0;
	while (noCarriage && flagUART_RX){
		while (!(UCSROA & (1 << RXC0)) && flagUART_RX);
		recChar = UDR0;
		if (recChar=='.'){
			recString[strLoc]='\0';
			noCarriage=fFalse;
			if (!strcmp(recString,"NONE")){//pull date and time from EEPROM
				
				Print0("ACKNONE");	//reply with what we got.
		{
	}
}

/*************************************************************************************************************/

void printTimeDate(BOOL pTime,BOOL pDate){
	if (pTime){
		char tempTime[11];
		strcpy(tempTime,currentTime.getTime());
		Print0(tempTime);
	}
	if (pDate){
		if (pTime){
			Print0(", ");
		}		
		char tempDate[17];
		strcpy(tempDate,currentTime.getDate());
		Print0(tempDate);
	}
	Print0(". ");
}
/*************************************************************************************************************/

void GoToSleep(BOOL shortOrLong){
		sei();
		volatile int sleepTime, sleepTicks = 0;
		//If bool is true, we are in low power mode/backup, sleep for 60 seconds then check ADC again
		if (shortOrLong == fTrue){
			sleepTime = SLEEP_TICKS_LOWV;
		} else {
			sleepTime = SLEEP_TICKS_HIGHV;
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
		Wait_ms(1);
		
		//Done sleeping, turn off sleeping led
		prtSLEEPled &= ~(1 << bnSLEEPled);
		prtSTATUSled |= (1 << bnSTATUSled);
}
/*************************************************************************************************************/

BOOL TakeADC(){
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
	
	//Disable ADC hardware/registers
	ADCSRA = 0;
	ADMUX = 0;
	DIDR0 |= (1 << ADC0D);
	
	//Turn off power
	PRR0 |= (1 << PRADC);
	
	//Re-enable interrupts
	sei();
	
	//Do work
	Wait_ms(5);

	flagGoodVolts = (adcReading < LOW_BATT_ADC) ? fFalse : fTrue;
}

/*************************************************************************************************************/

BOOL GetTemp(){
	WORD rawTemp = 0;
	
	//Power on temp monitor, let it settle
	prtTEMPen |= (1 << bnTEMPen);
	Wait_ms(20);
	
	//Turn SPI on
	PRR0 &= ~(1 << PRSPI);
	Wait_ms(5);
	
	//Slave select goes low to signal start of transmission
	prtSpi0 &= ~(1 << bnSS0);
	
	//Write to buffer
	UDR0 = 0x00;
	
	//Wit for data to be receieved.
	while (!(UCSR0A & (1 << RXC0)));
	rawTemp = (UDR0 << 8);
	UDR0 = 0x00;
	while (!(UCSR0A & (1 << RXC0)));
	rawTemp |= UDR0;
	
	//Turn off temp monitor
	prtTEMPen &= ~(1 << bnTEMPen);
	//Set flag to correct value.
	flagGoodTemp = (rawTemp < HIGH_TEMP) ? fFalse : fTrue;
}

/*--------------------------END-Public Funtions--------------------------------------------------------------------------------*/
