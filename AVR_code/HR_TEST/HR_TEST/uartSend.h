/*******************************************************************************\
| uart0.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 3/3/2013
| Last Revised: 3/3/2013
| Copyright of Todd Sukolsky
|================================================================================
| Description: This file conatians the UART0 and UART1 basic send routines used 
|			by the Atmel family of microcontrollers
|--------------------------------------------------------------------------------
| Revisions: 3/3: Initial build
|================================================================================
| *NOTES:
\*******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "stdtypes.h"

/**************************************************************************************************************/
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
/*
void PutUart1Ch(char ch){
	while (!(UCSR1A & (1 << UDRE1)));
	UDR1=ch;
}*/
/*************************************************************************************************************/
/*
void Print1(char string[]){
	BYTE i=0;
	while (string[i]){
		PutUart1Ch(string[i++]);
	}
}*/
/*************************************************************************************************************/