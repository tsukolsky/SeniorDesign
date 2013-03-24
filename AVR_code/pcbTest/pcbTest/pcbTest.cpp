/*******************************************************************************************\
| pcbTest.cpp
| Author: Todd Sukolsky
| Initial Build: 3/23/2013
| Last Revised: 3/23/13
| Copyright of Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description: This is the main program file for PCB testing. First tests the WAVR
|--------------------------------------------------------------------------------
| Revisions: 3/23-Initial Build. Testing 644PA->WAVR
|================================================================================
| *NOTES:
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
#include <avr/io.h>

#define GAVR

#include "ATmegaXX4PA.h"

//#include "ATmega2560.h"


using namespace std;


//USART/BAUD defines
#define FOSC 20000000		//20 MHz
#define ASY_OSC	32768		//32.768 kHz
#define BAUD 9600
#define MYUBRR FOSC/16/BAUD - 1			//declares baud rate

//Forward Declarations
void DeviceInit();
void AppInit(unsigned int ubrr);
void EnableRTCTimer();
void Wait_ms(volatile int delay);
void GoToSleep();

/*--------------------------Interrupt Service Routines------------------------------------------------------------------------------------*/
ISR(TIMER2_OVF_vect){
	static unsigned int counter=4;	
	prtSLEEPled ^= (1 << bnSLEEPled);	//toggles sleep led at .5Hz
	
/*	prtENABLE ^= (1 << counter);
	if (counter == 7){prtMAINen |= (1 << bnMAINen);counter++;}
	else if (counter == 8){prtMAINen &= ~(1 << bnMAINen); counter=4;}
	else;*/
	
	/*
	prtDEBUGled ^= (1 << counter);
	if (counter++ == 7){counter = 0;}
	*/

}

int main(void)
{
	DeviceInit();
	AppInit(MYUBRR);
	Wait_ms(200);
	EnableRTCTimer();
    while(1)
    {
		prtTEMPen |= (1 << bnTEMPen);
		Wait_ms(2000);
		prtTEMPen &= ~(1 << bnTEMPen);
        GoToSleep(); 
    }
}

/*************************************************************************************************************/
void DeviceInit(){
	//Set all ports to input with no pull
	DDRA = 0;
	DDRB = 0;
	DDRC = 0;
	DDRD = 0;
/*
	DDRE = 0;
	DDRF = 0;
	DDRG = 0;
	DDRH = 0;
	DDRJ = 0;
	DDRK = 0;
	DDRL = 0;
	
	PORTE = 0;
	PORTF = 0;
	PORTG = 0;
	PORTH = 0;
	PORTJ = 0;
	PORTK = 0;
	PORTL = 0;
	*/
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


		//Enable LEDs
		ddrSLEEPled |= (1 << bnSLEEPled);
		ddrSTATUSled |= (1 << bnSTATUSled);
		prtSLEEPled &= ~(1 << bnSLEEPled);		//turn on initially
		prtSTATUSled |= (1 << bnSTATUSled);
		
		//Enable all regulators!
		ddrENABLE |= (1 << bnGPSen)|(1 << bnBBen)|(1 << bnGAVRen)|(1 << bnGPSen);
		ddrTEMPen |= (1 << bnTEMPen);
		ddrMAINen |= (1 << bnMAINen);
		prtENABLE |= (1 << bnGPSen)|(1 << bnBBen)|(1 << bnGAVRen)|(1 << bnLCDen);
	//	prtTEMPen |= (1 << bnTEMPen);
	//	prtMAINen |= (1 << bnMAINen);
/*
		//Enable LEDs
		ddrDEBUGled |= 0xFF;
		prtDEBUGled = 0x00;
		ddrDEBUGled2 |= (1 << bnDBG10)|(1 << bnDBG9)|(1 << bnDBG8);
		prtDEBUGled2 |= ((1 << bnDBG10)|(1 << bnDBG9)|(1 << bnDBG8));
*/
	sei();
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
void GoToSleep(){
	sei();
	volatile int sleepTime=5, sleepTicks = 0;
	//If bool is true, we are in low power mode/backup, sleep for 60 seconds then check ADC again
	
	//Set to power save, then enable
	SMCR = (1 << SM1)|(1 << SM0);
	SMCR |= (1 << SE);
	
	//Give time to registers
	Wait_ms(1);
	//Go to sleep
	while (sleepTicks < sleepTime){
		asm volatile("SLEEP");
		sleepTicks++;
	} //endwhile
	
	//Give it time to power back on
	Wait_ms(10);
	
}
/*************************************************************************************************************/