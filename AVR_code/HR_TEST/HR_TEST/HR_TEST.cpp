/*******************************************************************************\
| HR_TEST.cpp
| Author: Todd Sukolsky
| Initial Build: 2/20/2013
| Last Revised: 2/26/13
| Copyright of Todd Sukolsky and Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description: The point of this module is to test the Pulse HR Sensor with the
|				atmega328 instead of an arduino.
|--------------------------------------------------------------------------------
| Revisions: (1) Need to move this into a Flag oriented procedure. After initial testing
|				 I will. Then step is to integrate with existing GAVR code.
|	     2/26: Got Speed Sensor working. Working on HR.
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
#include <avr/io.h>
#include "stdtypes.h"
#include "trip.h"

#define prtLED PORTC
#define ddrLED DDRC
#define bnLED  5
#define bnSPEEDLED  4

//Timer Overflow define for speed
#define TIMER1_OFFSET 65535

#define BAD_SPEED_THRESH 4
#define CALC_SPEED_THRESH 3		//at 20MPH, 4 interrupts take 1 second w/clk, should be at least every second

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
void PutUart0Ch(char ch);
void Print0(char string[]);
void powerDown();

//Globals
//IBI=ms inbetween beats; BPM=beats per minute; signal =adc reading. P=peak, T=trough, thresh=threshold, amp=amplitude
volatile WORD numberOfSpeedOverflows=0;
volatile unsigned int BPM, IBI;
volatile BOOL QS=fFalse, flagCalcSpeed=fFalse, flagNoSpeed=fTrue, dead=fFalse;
volatile WORD N=0;
volatile BOOL firstBeat=fTrue, secondBeat=fTrue, pulse=fFalse;
volatile WORD rate[10];

//Global trip 
trip globalTrip;

//ISR
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
		flagCalcSpeed=fTrue;
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


//Toggled every 4ms roughly. 1/(8MHz/128/248)
ISR(TIMER0_COMPA_vect){
	cli();
	//Declare variables
	WORD signal=0;
	volatile static unsigned int rate[10],P=512,T=512,thresh=512,amp=100;
	volatile static WORD N=0;
	
	//INcrement time since last timer interrupt. Assumes it doesnt miss any hits, small error
	N+=2;
	
	//Implementation: Should be moved to a routine/function in main program where this sends flag up.
	signal = GetADC();		//retrieves ADC reading on ADC0
	
	//Adjust Peak and Trough Accordingly
	if (signal < thresh && N > (IBI/5)*3){		//signals less than thresh, time inbetween is more than last interval * 3/5
		if (signal < T){
			T = signal;
		}
	}
	if (signal > thresh && signal > P){
		P = signal;
	}
	
	//If time since last read is more than 250, see if signal is above thresh and time is good.
	if (N>250){
		if ((signal > thresh) && !pulse && (N>((IBI/5)*3)) && !firstBeat){	//send pulse high
			Print0("-BEAT-");
			pulse=fTrue;
			prtLED |= (1 << bnLED);		//turn LED on
			IBI=N;
			N=0;
			if (secondBeat){
				secondBeat=fFalse;
				for (int i=0; i < 9; i++){
					rate[i]=IBI;
				}
			}
		
			//Calculate the IBI and BPM.
			volatile WORD runningTotal=0;
			for (int i=0; i< 9; i++){
				rate[i]=rate[i+1];	//shift backwards
				runningTotal += rate[i];
			}
			rate[9]=IBI;
			runningTotal+=rate[9];
			runningTotal/=10;			//time it took all of them in milliseconds
			BPM=60000/runningTotal;		//60 seconds in minute, 1000ms in second
			QS=fTrue;

		} else if (firstBeat){
				firstBeat=fFalse;
		}		
	}//end if N>250
		
	//No pulse after last interrupt/pulse, send signal low again, reset things.
	if (signal < thresh && pulse){
		prtLED &= ~(1 << bnLED);
		pulse=fFalse;
		amp=P-T;
		thresh=amp/2+T;
		P=thresh;
		T=thresh;
	}
	
	//Wow, not getting a pulse, reset things
	if (N>=20000){
		Print0("-TIMEOUT-");
		thresh=512;
		P=512;
		T=512;
		firstBeat=fTrue;
		secondBeat=fTrue;
		N=0;
	}
	sei();		//dumb as shit
}

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
		if (QS){
			cli();
			Print0("Processing...");
			QS=fFalse;
			char BMPstring[10];
			char IBIstring[10];
			utoa(BPM,BMPstring,10);
			utoa(IBI,IBIstring,10);
			BMPstring[9]='\0';
			BMPstring[8]='.';
			Print0("BPM:");
			Print0(BMPstring);
			sei();
		}
		
		if (flagCalcSpeed){
			cli();
			//Calculate speed using data points.
			float speed;
			char speedString[8];
			speed = globalTrip.getCurrentSpeed();
			dtostrf(speed,5,2,speedString);
			speedString[6]='.';
			speedString[7]='\0';
			Print0("Speed:");
			Print0(speedString);
			flagCalcSpeed=fFalse;
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
	ADCSRA |= (1 << ADEN)|(1 << ADPS1)|(1 << ADPS0);		//enable ADC with clock division factor of 8
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
	TCCR0B = (1 << CS02)|(1 << CS00);	//clk/128
	OCR0A = 0x7c;						//248
	TCNT0 = 0x00;						//Initialize
	//TIMSK0 = (1 << OCIE0A);			//enable OCIE2A
}


/*************************************************************************************************************/

WORD GetADC(){
	//Disable global interrupts; declare variables.
	//cli();		//GLOBAL INTERRUPTS ALREADY DISABLED
	volatile WORD ADCreading=0;
	volatile static unsigned int reps=0;
	
	//Take two ADC readings, throw the first one out.
	for (int i=0; i<2; i++){ADCSRA |= (1 << ADSC); while (ADCSRA & (1 << ADSC));} //does two

	//Get the last ADC reading.	
	ADCreading = ADCL;
	ADCreading |= (ADCH << 8);
	
	/*****Debugging******/
	if (reps++>50){
		char tempString[10];
		utoa(ADCreading,tempString,10);
		tempString[8]='-';	
		tempString[9]='\0';
		Print0(tempString);
		reps=0;
	}	
	/********************/
	
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
void PutUart0Ch(char ch){
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0=ch;
}
/*************************************************************************************************************/
void Print0(char string[]){
	BYTE i=0;
	
	while (string[i]){
		PutUart0Ch(string[i++]);
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
	
