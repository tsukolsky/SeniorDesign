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
#define bnSPEEDLED  4

//Timer Overflow define for speed
#define TIMER_OFFSET 65535
#define TIMER1_CLOCK_sec .000032
#define WHEEL_DISTANCE .0013882576	//87.96" circumference in miles
#define SECONDS_IN_HOUR 3600

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
volatile BOOL QS=fFalse, flagCalcSpeed=fFalse;;
volatile int speedPoints[5];

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
	volatile static BOOL firstPoint=fTrue;
	volatile static unsigned int lastTime=0,interruptsSinceLastCalc=0;
	volatile unsigned int newTime=0;

	unsigned int value=TCNT1;

	newTime=value;

	if (interruptsSinceLastCalc++ > 5){
		flagCalcSpeed=fTrue;
		interruptsSinceLastCalc=0;
	}
	
	prtLED |= (1 << bnSPEEDLED);
	Wait_ms(10);
	prtLED &= ~(1 << bnSPEEDLED);
	
	char tempString1[10];
	
	if (firstPoint){
		if (newTime < lastTime){
			newTime+=TIMER_OFFSET;
		}		
		for (int i=0; i< 5; i++){
			speedPoints[i]=newTime-lastTime;
		}
		firstPoint=fFalse;
	} else {
		for (int i=0; i<5; i++){
			speedPoints[i]=speedPoints[i+1];	//shift everything down.
		}
		if (newTime < lastTime){
			newTime+=TIMER_OFFSET;
		}		
		speedPoints[4]=newTime-lastTime;
	}
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
	Print0("Hello...");
	while (fTrue){
		//while(flagSampleADC
		int sample=GetADC();
		Wait_ms(100);
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
			unsigned int value=TCNT1;
			//Calculate speed using data points.
			float sum=0;
			float speed=0;
			char tempString[8];
			for (int i=0; i<5; i++){
				sum+=speedPoints[i];
			}
			speed=SECONDS_IN_HOUR*WHEEL_DISTANCE/(sum*TIMER1_CLOCK_sec/10);
			dtostrf(speed,5,2,tempString);
			tempString[6]='.';
			tempString[7]='\0';
			Print0("Speed:");
			Print0(tempString);
			flagCalcSpeed=fFalse;
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
	PRR |= (1 << PRTWI)|(1 << PRADC)|(1 << PRSPI);  //Turn EVERYTHING off initially except USART0(UART0) and TIM2

	//Initialize timer 2, counter compare on TCNTA compare equals
	TCCR2A = (1 << WGM21);				//OCRA good, TOV set on top. TCNT2 cleared when match occurs
	TCCR2B = (1 << CS22)|(1 << CS20);	//clk/128
	OCR2A = 0x7c;		//124
	//TIMSK2 = (1 << OCIE2A);				//enable OCIE2A
	
	//Initialize Timre 1, counter is read on an interrupt to measure speed. assumes rider is going  above a certain speed for initial test.
	/*TCCR1B |= (1 << CS12); //Prescaler of 256 for system clock
	TIFR1= (1 << TOV2);
	TIMSK1=(1 << TOIE2);
	TCNT1=0;*/
	
	//Enable SPeed interrupt
	EICRA = (1 << ISC01)|(1 << ISC00);
	//EIMSK = (1 << INT0);
	
	//87.96" distance travelled on normal 28" bicycle wheel
	//speed = distance/time; 87.96" = .0013882576 miles
	//20mph/distance=14406= 1/time => time=249.88ms; therefore, have the thing divide by 2343.75 for a good number. If 1MHz clock, could divide by 512 and get somewhat close. 
	//Setup LED Blinking Port
	ddrLED |= (1 << bnLED)|(1 << bnSPEEDLED);
	prtLED &= ~((1 << bnSPEEDLED)|(1 << bnLED));	//off initially.
	
	//Enable Global Interrupts. 
	sei();	
	
}

/*************************************************************************************************************/

int GetADC(){
	//Pruple wire connected to ADC0
	static int times=0;
	cli();
	volatile WORD ADCreading=0;
	PRR &= ~(1 << PRADC);	//give power back to adc
	ADMUX |= (1 << REFS0)|(1 << MUX1);	//internal 3.3V reference on AVCC, channel ADC2
	DIDR0 = (1 << ADC5D)|(1 << ADC4D)|(1 << ADC3D)|(0 << ADC2D)|(1 << ADC1D)|(1 << ADC0D);	//disable all ADC except for ADC0
	Wait_ms(10);
	for (int i=0; i<2; i++){ADCSRA |= (1 << ADSC); while (ADCSRA & (1 << ADSC));} //does two
	
	ADCreading = ADCL;
	ADCreading |= (ADCH << 8);
	
	if (times++>=10){
		char tempString[10];
		utoa(ADCreading,tempString,10);
		tempString[9]='\0';
		Print0(tempString);
		times=0;
	}
	
	DIDR0 |= (1 << ADC0D);
	PRR |= (1 << PRADC);
	ADMUX=0;
	ADCSRA=0;
	
	return ADCreading;
	sei();
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
	