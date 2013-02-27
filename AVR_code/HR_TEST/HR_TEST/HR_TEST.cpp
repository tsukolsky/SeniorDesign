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
|             87.96" = .0013882576 miles 20mph/distance=14406= 1/time => time=249.88ms; 
|             Timer two because 8MHz/256=31.25kHz clock => .032ms per tick.
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
#include "odometerClass.h"

#define prtLED PORTC
#define ddrLED DDRC
#define bnLED  5
#define bnSPEEDLED  4

//Timer Overflow define for speed
#define TIMER_OFFSET 65535

//Baud/UART defines
#define FOSC 8000000
#define BAUD 9600
#define MYUBRR FOSC/16/BAUD-1

void DeviceInit();
void AppInit(unsigned int ubrr);
WORD GetADC();
void Wait_ms(int delay);
void PutUart0Ch(char ch);
void Print0(char string[]);

//Globals
//IBI=ms inbetween beats; BPM=beats per minute; signal =adc reading. P=peak, T=trough, thresh=threshold, amp=amplitude
volatile int BPM, IBI;
volatile BOOL QS=fFalse, flagCalcSpeed=fFalse;

odometer odometer1();

//ISR
ISR(TIMER1_OVF_vect){
	cli();
	//Do nothing;
	prtLED |= (1 << bnLED);
	Wait_ms(10);
	prtLED &= ~(1 << bnLED);
	sei();
}

ISR(INT0_vect){
	cli();
	volatile static unsigned int lastTime=0,interruptsSinceLastCalc=0;
	volatile unsigned int newTime=0;

	unsigned int value=TCNT1;

	newTime=value;

	if (interruptsSinceLastCalc++ > 8){
		flagCalcSpeed=fTrue;
		interruptsSinceLastCalc=0;
	}
	
	prtLED |= (1 << bnSPEEDLED);
	Wait_ms(10);
	prtLED &= ~(1 << bnSPEEDLED);
	
	if (newTime < lastTime){
		odometer1.addNewDataPoint(newTime+TIMER_OFFSET-lastTime);
	} else {
		odometer1.addNewDataPoint(newTime-lastTime);
	}

	//Update last time
	lastTime=newTime;

	sei();	
}


//Toggled every 2ms roughly. 1/(8MHz/128/124)
ISR(TIMER2_COMPA_vect){
	cli();
	//Declare variables
	WORD signal=0;
	volatile static int rate[10],P=512,T=512,thresh=512,amp=100;
	volatile static unsigned long sampleCounter=0, lastBeatTime=0;
	volatile static BOOL pulse=fFalse,firstBeat=fTrue,secondBeat=fTrue;
	
	//Implementation: Should be moved to a routine/function in main program where this sends flag up.
	signal = GetADC();		//retrieves ADC reading on ADC0
	sampleCounter += 2;
	int N = sampleCounter - lastBeatTime;
	
	//Adjust Peak and Trough Accordingly
	if (signal < thresh && N > (IBI/5)*3){		//signals less than thresh, time inbetween is more than last interval * 3/5
		if (signal < T){
			T = signal;
		}
	}
	if (signal > thresh && signal > P){
		P = signal;
	}
	
	//If time since alst read is more than 250, see if signal is above thresh and time is good.
	if (N>250){
		if ((signal > thresh) && !pulse && (N>((IBI/5)*3))){	//send pulse high
			pulse=fTrue;
			prtLED |= (1 << bnLED);		//turn LED on
			IBI=N;
			lastBeatTime=sampleCounter;
		}
	
		//If first or second beat, act accordingly
		if (firstBeat){
			firstBeat=fFalse;
			return;
		}
		if (secondBeat){
			secondBeat=fFalse;
			for (int i=0; i < 10; i++){
				rate[i]=IBI;
			}
		}
	
		//Calculate the IBI and BPM.
		WORD runningTotal=0;
		for (int i=0; i< 9; i++){
			rate[i]=rate[i+1];	//shift backwards
			runningTotal += rate[i];
		}
		rate[9]=IBI;
		runningTotal+=rate[9];
		runningTotal/=10;			//time it took all of them in milliseconds
		BPM=60000/runningTotal;		//60 seconds in minute, 1000ms in second
		QS=fTrue;
		lastBeatTime=sampleCounter;
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
	if (N>2500){
		thresh=512;
		P=512;
		T=512;
		firstBeat=fTrue;
		secondBeat=fTrue;
		lastBeatTime=sampleCounter;
	}
	sei();		//dumb as shit
}

//Main Program
int main(void){
	DeviceInit();
	AppInit(MYUBRR);
	Print0("Hello...");
	while (fTrue){

		if (QS){
			//Print0("Processing...");
			QS=fFalse;
			char BMPstring[10];
			char IBIstring[10];
			utoa(BPM,BMPstring,10);
			utoa(IBI,IBIstring,10);
			BMPstring[9]='\0';
			BMPstring[8]='.';
			//Print0("BPM:");
			//Print0(BMPstring);
			IBIstring[9]='\0';
			IBIstring[8]='.';
			//Print0("IBI:");
			//Print0(IBIstring);	
			//Wait_ms(20);
		}
		
		if (flagCalcSpeed){
			//Calculate speed using data points.
			float speed;
			char speedString[8];
			speed = odometer1.getCurrentSpeed();
			dtostrf(speed,5,2,tempString);
			speedString[6]='.';
			speedString[7]='\0';
			Print0("Speed:");
			Print0(speedString);
			flagCalcSpeed=fFalse;
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
	
	//Set high and low byte of baud rate, then enable pins and functions
	UBRR0L = ubrr;
	UBRR0H |= (ubrr >> 8);
	UCSR0B = (1 << TXEN0)|(1 << RXEN0);		//Enable TX0 and RX0
	UCSR0C = (1 << UCSZ01)|(1 << UCSZ00);		//Async, 8 data bits no parity
	
	//Disable power to certain modules
	PRR |= (1 << PRTWI)|(1 << PRTIM0)|(1 << PRSPI);  //Turn off everything 

	ADCSRA |= (1 << ADEN);				//enable ADC
	ADMUX |= (1 << REFS0)|(1 << MUX1);		//internal 3.3V reference on AVCC, channel ADC2

	//Initialize timer 2, counter compare on TCNTA compare equals
	TCCR2A = (1 << WGM21);				//OCRA good, TOV set on top. TCNT2 cleared when match occurs
	TCCR2B = (1 << CS22)|(1 << CS20);		//clk/128
	OCR2A = 0x7c;					//124
	TCNT2 = 0x00;					//Initialize
	//TIMSK2 = (1 << OCIE2A);				//enable OCIE2A
	
	//Initialize Timer 1(16-bit), counter is read on an interrupt to measure speed. assumes rider is going  above a certain speed for initial test.
	TCCR1B |= (1 << CS12); 				//Prescaler of 256 for system clock
	TIFR1= (1 << TOV2);				//Make sure the overflow flag is not already set
	TCNT1 = 0x00;
	//TIMSK1=(1 << TOIE2);

	
	//Enable SPeed interrupt
	EICRA = (1 << ISC01)|(1 << ISC00);
	//EIMSK = (1 << INT0);
	
	//Setup LED Blinking Port
	ddrLED |= (1 << bnLED)|(1 << bnSPEEDLED);
	prtLED &= ~((1 << bnSPEEDLED)|(1 << bnLED));	//off initially.
	
	//Enable Global Interrupts. 
	sei();	
}

/*************************************************************************************************************/

WORD GetADC(){
	//Disable global interrupts; declare variables.
	cli();
	static int times=0;
	volatile WORD ADCreading=0;
	
	//Take two ADC readings, throw the first one out.
	for (int i=0; i<2; i++){ADCSRA |= (1 << ADSC); while (ADCSRA & (1 << ADSC));} //does two

	//Get the last ADC reading.	
	ADCreading = ADCL;
	ADCreading |= (ADCH << 8);
	
	if (times++>=10){
		char tempString[10];
		utoa(ADCreading,tempString,10);
		tempString[9]='\0';
		Print0(tempString);
		times=0;
	}
	
	//Re-enable global interrupts. 
	sei();
	return ADCreading;
}
	
/*************************************************************************************************************/
void Wait_ms(int delay){
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
	
