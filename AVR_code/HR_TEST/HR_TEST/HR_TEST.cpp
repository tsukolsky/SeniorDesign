/*******************************************************************************\
| HR_TEST.cpp
| Author: Todd Sukolsky
| Initial Build: 2/20/2013
| Last Revised: 3/1/13
| Copyright of Todd Sukolsky and Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description: The point of this module is to test the Pulse HR Sensor with the
|				atmega328 instead of an arduino.
|--------------------------------------------------------------------------------
| Revisions: (1) Need to move this into a Flag oriented procedure. After initial testing
|				 I will. Then step is to integrate with existing GAVR code.
|	     2/26: Got Speed Sensor working. Working on HR.
|		  3/1: Moved HR Functionality/logic into "heartMonitor" class. See "heartMonitor.h"
|	
|================================================================================
| *NOTES: (1) 87.96" distance travelled on normal 28" bicycle wheel speed = distance/time; 
|             87.96" = .0013882576 miles 20mph/distance=14406= 1/time => time=249.88ms; One mph = 4.96 S=> 156,150 in counter = 2.4  
|             Timer two because 8MHz/256=31.25kHz clock => .032ms per tick. => at 20mph, counter will hit ~7812.
|
\*******************************************************************************/

using namespace std;

#include <inttypes.h>
#include <math.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stdtypes.h"
#include "trip.h"
#include "uartSend.h"

#define prtLED PORTC
#define ddrLED DDRC
#define bnLED  5
#define bnSPEEDLED  4

//Timer Overflow define for speed
#define TIMER1_OFFSET 65535

#define BAD_SPEED_THRESH 4
#define CALC_SPEED_THRESH 3		//at 20MPH, 4 interrupts take 1 second w/clk, should be at least every second
#define MINIMUM_HR_THRESH 400	//
#define MAXIMUM_HR_THRESH 700	//seen in testing it usually doesn't go more than 100 above or below 512 ~ 1.67V

#define NUM_MS	1				//4 ms
#define ONE_MS 0xFF			//32 ticks on 8MHz/256 is 1ms


//Baud/UART defines
#define FOSC 8000000
#define BAUD 9600
#define MYUBRR FOSC/16/BAUD-1

void DeviceInit();
void AppInit(unsigned int ubrr);
void initHRSensing();
void initSpeedSensing();
WORD GetADC();
void Wait_ms(volatile int delay);

void powerDown();

//Globals
//IBI=ms inbetween beats; BPM=beats per minute; signal =adc reading. P=peak, T=trough, thresh=threshold, amp=amplitude
volatile WORD numberOfSpeedOverflows=0;
volatile BOOL QS=fFalse, flagNoSpeed=fTrue, dead=fFalse;
BOOL flagUpdateUserStats=fFalse;

//Global trip 
trip globalTrip;


/*-------------------------------------------------------------------ISR------------------------------------------------------------------------------*/
ISR(TIMER1_OVF_vect){
	cli();
	//Check to see if we are going to slow to care.
	if (numberOfSpeedOverflows++ > BAD_SPEED_THRESH && !flagNoSpeed){
		//Let the INT0 know that on next interrupt it shouldn't calc speed, but initialize "speedPoints" in odometer class.
		flagNoSpeed=fTrue;
		globalTrip.resetSpeedPoints();
	}

	//SHow me that is happened with LED;
	prtLED |= (1 << bnLED);
	Wait_ms(2);
	prtLED &= ~(1 << bnLED);
	
	sei();
}

//This is hit when there is a speed magnet hit.
ISR(INT0_vect){
	cli();
	volatile static unsigned int interruptsSinceLastCalc=0;
	unsigned int value=TCNT1;

	/**********************************************************************************************/
	//THis should be replaced by a flag that every second is sent high to calculate the speed. At 16MHz, will hit before an issue
	//happens. Could also just have screen pull speed data before updating screen.
	if (interruptsSinceLastCalc++ > CALC_SPEED_THRESH){
		flagUpdateUserStats=fTrue;
		interruptsSinceLastCalc=0;
	}
	/**********************************************************************************************/
	
	//Show me that it saw an interrupt.
	prtLED |= (1 << bnSPEEDLED);
	Wait_ms(2);
	prtLED &= ~(1 << bnSPEEDLED);
	
	if (flagNoSpeed){
		//Something to alert that speed was 0 and print it to screen.
		interruptsSinceLastCalc=0;
		flagNoSpeed=fFalse;
		globalTrip.resetSpeedPoints();
	} else {
		globalTrip.addSpeedDataPoint(value+(numberOfSpeedOverflows*TIMER1_OFFSET));
	}

	//Reset TCNT1
	TCNT1=0x00;
	sei();	
}


//Toggled every 4ms roughly. 1/(8MHz/128/248) * 2
ISR(TIMER0_COMPA_vect){
	cli();
	//Declare variables
	WORD signal=0;
	volatile static unsigned int N=0;
	
	//Increment N (time should reflect number of ms between timer interrupts), get ADC reading, see if newSample is good for anything.
	N+=4;
	signal = GetADC();		//retrieves ADC reading on ADC0
	if (signal > MINIMUM_HR_THRESH && signal < MAXIMUM_HR_THRESH){
		globalTrip.newHRSample(signal, 4);
		N=0;
	}	

	//Re-enable interrupts
	sei();
}

/*-------------------------------------------------------------------END-ISR------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------MAIN---------------------------------------------------------------------------------*/

//Main Program
int main(void){
	DeviceInit();
	AppInit(MYUBRR);
	Print0("Hello...");
	Wait_ms(500);
	initHRSensing();	
	initSpeedSensing();
	sei();
	while (fTrue){
		if (flagUpdateUserStats){
			cli();
			//Declare variables
			double speed, currentHR;
			char speedString[8], BPMstring[8];
			
			//Do work on HR
			Print0("Processing...");
			currentHR = globalTrip.getCurrentHR();
			dtostrf(currentHR, 5, 2, BPMstring);
			BPMstring[9]='\0';
			BPMstring[8]='.';
			Print0("BPM:");
			Print0(BPMstring);
			PutUart0Ch('-');
/*
			//Calculate speed using data points.
			speed = globalTrip.getCurrentSpeed();
			dtostrf(speed,5,2,speedString);
			speedString[6]='.';
			speedString[7]='\0';
			Print0("Speed:");
			Print0(speedString);*/
			//Reset Flag
			flagUpdateUserStats=fFalse;
			//Wait_ms(1000);
			sei();
		}//end calc speed	
		
		if (dead){
			return 1; 
			//Setup to sleep
			powerDown();
		}
	}//end while True	
	return 0;
}//end main

/*-------------------------------------------------------------------END-MAIN------------------------------------------------------------------------------*/

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
	
	//Set high and low byte of baud rate, then enable pins and functions
	UBRR0L = ubrr;
	UBRR0H |= (ubrr >> 8);
	UCSR0B = (1 << TXEN0)|(1 << RXEN0);		//Enable TX0 and RX0
	UCSR0C = (1 << UCSZ01)|(1 << UCSZ00);		//Async, 8 data bits no parity
	
	//Disable power to certain modules
	PRR |= (1 << PRTWI)|(1 << PRTIM2)|(1 << PRSPI);  //Turn off everything 

	//Setup ADC
	ADCSRA |= (1 << ADEN)|(1 << ADPS1);		//enable ADC with clock division factor of 8
	ADMUX |= (1 << REFS0)|(1 << MUX1);		//internal 3.3V reference on AVCC, channel ADC2
	
	//Setup LED Blinking Port
	ddrLED |= (1 << bnLED)|(1 << bnSPEEDLED);
	prtLED &= ~((1 << bnSPEEDLED)|(1 << bnLED));	//off initially.
		
}
/*************************************************************************************************************/
void initSpeedSensing(){
	//Initialize Timer 1(16-bit), counter is read on an interrupt to measure speed. assumes rider is going  above a certain speed for initial test.
	TCCR1B |= (1 << CS12); 				//Prescaler of 256 for system clock
	TIFR1= (1 << TOV2);					//Make sure the overflow flag is not already set
	TCNT1 = 0x00;
	TIMSK1=(1 << TOIE2);
	
	//Enable SPeed interrupt
	EICRA = (1 << ISC01)|(1 << ISC00);
	//EIMSK = (1 << INT0);
	
}

/*************************************************************************************************************/
void initHRSensing(){
	//Initialize timer 2, counter compare on TCNTA compare equals
	TCCR0A = (1 << WGM01);				//OCRA good, TOV set on top. TCNT2 cleared when match occurs
	TCCR0B = (1 << CS02);				//clk/256
	OCR0A = ONE_MS*NUM_MS;					//Number of Milliseconds
	TCNT0 = 0x00;						//Initialize
	TIMSK0 = (1 << OCIE0A);			//enable OCIE2A
}


/*************************************************************************************************************/

WORD GetADC(){
	//Disable global interrupts; declare variables.
	//cli();		//GLOBAL INTERRUPTS ALREADY DISABLED
	volatile WORD ADCreading=0;
	volatile static WORD reps=0;
	
	//Take two ADC readings, throw the first one out.
	//for (int i=0; i<2; i++){ADCSRA |= (1 << ADSC); while (ADCSRA & (1 << ADSC));} //does two
	ADCSRA |= (1 << ADSC);
	while(ADCSRA & (1 << ADSC));
	
	//Get the last ADC reading.	
	ADCreading = ADCL;
	ADCreading |= (ADCH << 8);
	
/*	if (reps++>500){
		flagUpdateUserStats=fTrue;
		reps=0;
	}*/
	/*****Debugging******
	if (reps++>2){
		char tempString[10];
		utoa(ADCreading,tempString,10);
		tempString[8]='-';	
		tempString[9]='\0';
		PutUart0Ch('-');
		Print0(tempString);
		reps=0;
	}	
	********************/
	
	//Re-enable global interrupts. 
	//sei();
	return ADCreading;
}
	
/*************************************************************************************************************/
void Wait_ms(volatile int delay){
	volatile int i=0;
	while (delay > 0){
		for (i=0; i < 400; i++){
			asm volatile("nop");
		}
		delay--;
	}	
}
/*************************************************************************************************************/

void powerDown(){
	cli();
	SMCR = (1 << SM1);	//Power down.
	SMCR |= (1 << SE);	//Enable sleep
	Wait_ms(1);
	sei();	//allow it to be woken up on an int from WAVR.
	asm volatile("SLEEP");	//Go to sleep/power down
	
}
/*************************************************************************************************************/
	
