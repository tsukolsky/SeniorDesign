/*******************************************************************************\
| ATmegaXX4PA.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 1/3/2013
| Last Revised: 1/29/2013
| Copyright of Todd Sukolsky
|================================================================================
| Description:	This header contains pin declarations for the ATmegaXX4P(A) family,
|				including the 324P/A, 644P/A and 1284P.
|--------------------------------------------------------------------------------
| Revisions:	1/28- Revised for all XX4PA chips.
|				1/29- Added TIMER2 and STATUSled changes
|				2/2-- Added definitions for BB and GAVR alert lines, template for 
|					  regulator array.
|================================================================================
| *NOTES:
\*******************************************************************************/


/* ---------------------------------------------------------------
				Interface Connector Declarations
   --------------------------------------------------------------- */

//Interrupt Pins for GAVR
#define prtGAVRINT	PORTB
#define ddrGAVRINT	DDRB
#define pinGAVRINT	PINB
#define bnGAVRINT	3

//Debug LED Configurations
#define prtSLEEPled		PORTD
#define ddrSLEEPled		DDRD
#define pinSLEEPled		PIND
#define bnSLEEPled		7

#define prtSTATUSled	PORTC
#define ddrSTATUSled	DDRC
#define pinSTATUSled	PINC
#define bnSTATUSled		2

//Demos of BB and Graphics AVR alert pins, currently LEDS, will be interrupts that alert for shutdown.
#define prtBBleds		PORTA
#define ddrBBleds		DDRA
#define bnBBint			1
#define bnBBtemp		2

#define prtGAVRleds		PORTA
#define ddrGAVRleds		DDRA
#define bnGAVRint		3
#define bnGAVRtemp		4

//Enable pins for power
#define prtENABLE	PORTA
#define ddrENABLE	DDRA
#define bnLCDen		5
#define bnGAVRen	6
#define bnGPSen		7

#define prtBBen		PORTB
#define ddrBBen		DDRB
#define bnBBen		0	

#define prtTEMPen	PORTB
#define ddrTEMPen	DDRB
#define bnTEMPen	1	

// Symbol definitions for the I2C signals
#define prtSDA		PORTC
#define prtSCL		PORTC
#define ddrSDA		DDRC
#define ddrSCL		DDRC
#define pinSDA		PINC
#define pinSCL		PINC
#define bnSDA		1
#define bnSCL		0

// Symbol definitions for access to the UART
#define prtRXD		PORTD
#define prtTXD		PORTD
#define ddrRXD		DDRD
#define ddrTXD		DDRD
#define pinRXD		PIND
#define pinTXD		PIND
#define bnRXD		0
#define bnTXD		1

// Symbol definition for the master pin change interrupt into the microcontroller, on PB2 (INT2)
#define prtINT2		PORTB
#define ddrINT2		DDRB
#define pinINT2		PINB
#define bnINT2		2

// Symbol definitions for access to the SPI (0) connector
#define	prtSpi0		PORTB
#define	pinSpi0		PINB
#define	ddrSpi0		DDRB
#define	bnSS0		4
#define	bnMosi0		5
#define	bnMiso0		6
#define	bnSck0		7

//Symbol definitons for acces to SPI (1) connector for TI_ temp sensor, USART mode
#define prtSpi1		PORTD
#define ddrSpi1		DDRD
#define pinSpi1		PIND
#define bnSS1		5
#define bnSCK1		4
#define bnMOSI1		3	 
#define bnMISO1		2	


//Symbol definitions for ADC registers
#define REFS1 7
#define REFS0 6
#define ADLAR 5
#define MUX3  3
#define MUX2  2
#define MUX1  1
#define MUX0  0

#define ADEN  7
#define ADSC  6
#define ADATE 5
#define ADIF  4
#define ADIE  3
#define ADPS2 2
#define ADPS1 1
#define ADPS0 0

#define ACME  6
#define ADTS2 2
#define ADTS1 1
#define ADTS0 0

/***************************************************************/