/*******************************************************************************\
| HR_TEST.cpp
| Author: Todd Sukolsky
| Initial Build: 2/20/2013
| Last Revised: 2/20/13
| Copyright of Todd Sukolsky and Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description: The point of this module is to test the Pulse HR Sensor with the
|				atmega328 instead of an arduino.
|--------------------------------------------------------------------------------
| Revisions: (1) Need to move this into a Flag oriented procedure. After initial testing
|				 I will. Then step is to integrate with existing GAVR code.
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
#include "stdtypes.h"

#define prtLED PORTC
#define ddrLED DDRC
#define bnLED  5

//Timer Overflow define for speed
#define TIMER_OFFSET 1023
#define WHEEL_DISTANCE .0013882576	//87.96" circumference in miles

//Baud/UART defines
#define FOSC 8000000
#define BAUD 9600
#define MYUBRR FOSC/16/BAUD-1

void DeviceInit();
void AppInit(unsigned int ubrr);
int GetADC();
void Wait_ms(int delay);
void PutUart0Ch(char ch);
void Print0(char string[]);

//Globals
//IBI=ms inbetween beats; BPM=beats per minute; signal =adc reading. P=peak, T=trough, thresh=threshold, amp=amplitude
volatile int BPM, IBI;
volatile BOOL QS=fFalse;
volatile int speedPoints[10];

//ISR
ISR(INT0_vect){
	cli();
	volatile static BOOL firstPoint=fTrue;
	volatile static int lastTime=0;
	volatile int newTime=0;
	
	if (firstPoint){
		if (TCNT1 < lastTime){
			newTime=TCNT1+TIMER_OFFSET;
		} else {
			newTime=TCNT1;
		}
		for (int i=0; i< 10; i++){
			speedPoints[i]=newTime-lastTime;
		}
		firstPoint=fFalse;
	} else {
		for (int i=0; i<9; i++){
			speedPoints[i]=speedPoints[i+1];	//shift everything down.
		}
		if (TCNT1 < lastTime){
			newTime=TCNT1+TIMER_OFFSET;
		} else {
			newTime=TCNT1;
		}
		speedPoints[9]=newTime-lastTime;
	}
	lastTime=TCNT1;
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
			IBI=sampleCounter-lastBeatTime;
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
	
	while (fTrue){
		//while(flagSampleADC
		if (QS){
			Print0("Processing...");
			QS=fFalse;
			char BMPstring[10];
			char IBIstring[10];
			itoa(BPM,BMPstring,10);
			itoa(IBI,IBIstring,10);
			BMPstring[9]='\0';
			BMPstring[8]='.';
			Print0("BPM:");
			Print0(BMPstring);
			IBIstring[9]='\0';
			IBIstring[8]='.';
			Print0("IBI:");
			Print0(IBIstring);	
			Wait_ms(20);
		}
		//Needs external oscillator.
		//SMCR = (1 << SE);	//enable the idle
		//asm volatile("SLEEP");	//goes into idle state. wakes up on compare		
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
	UCSR0C = (1 << UCSZ01)|(1 << UCSZ00);	//Async, 8 data bits no parity
	
	//Disable power to certain modules
	PRR |= (1 << PRTWI)|(1 << PRTIM1)|(1 << PRTIM0)|(1 << PRADC)|(1 << PRSPI);  //Turn EVERYTHING off initially except USART0(UART0) and TIM2

	//Initialize timer 2, counter compare on TCNTA compare equals
	TCCR2A = (1 << WGM21);				//OCRA good, TOV set on top. TCNT2 cleared when match occurs
	TCCR2B = (1 << CS22)|(1 << CS20);	//clk/128
	OCR2A = 0x7c;		//124
	TIMSK2 = (1 << OCIE2A);				//enable OCIE2A
	
	//Initialize Timre 1, counter is read on an interrupt to measure speed. assumes rider is going  above a certain speed for initial test.
	TCCR1A = 0x00;	//normal mode, mode 0
	TCCR1B = (1 << CS12)|(1 << CS11); //Prescaler of 256 for system clock
	
	
	//87.96" distance travelled on normal 28" bicycle wheel
	//speed = distance/time; 87.96" = .0013882576 miles
	//20mph/distance=14406= 1/time => time=249.88ms; therefore, have the thing divide by 2343.75 for a good number. If 1MHz clock, could divide by 512 and get somewhat close. 
	//Setup LED Blinking Port
	ddrLED |= (1 << bnLED);
	prtLED &= ~(1 << bnLED);	//off initially.
	
	//Enable Global Interrupts. 
	sei();	
	
}

/*************************************************************************************************************/

int GetADC(){
	//Pruple wire connected to ADC0
	
	int ADCreading=0;
	PRR &= ~(1 << PRADC);	//give power back to adc
	ADMUX |= (1 << REFS1);	//internal 3.3V reference on AVCC, channel ADC0
	DIDR0 = (1 << ADC5D)|(1 << ADC4D)|(1 << ADC3D)|(0 << ADC2D)|(1 << ADC1D);	//disable all ADC except for ADC0
	for (int i=0; i<2; i++){ADCSRA |= (1 << ADSC); while (ADCSRA & (1 << ADSC));} //does two
	
	ADCreading = ADCL;
	ADCreading |= (ADCH << 8);
	
	DIDR0 |= (1 << ADC0D);
	PRR |= (1 << PRADC);
	ADMUX=0;
	ADCSRA=0;
	
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
	