/*******************************************************************************\
| ATmegaXX4PA.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 1/3/2013
| Last Revised: 3/25/2013
| Copyright of Todd Sukolsky and Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description:	This header contains pin declarations for the ATmegaXX4P(A) family,
|				including the 324P/A, 644P/A and 1284P.
|--------------------------------------------------------------------------------
| Revisions:	1/28- Revised for all XX4PA chips.
|				1/29- Added TIMER2 and STATUSled changes
|				2/2-- Added definitions for BB and GAVR alert lines, template for 
|					  regulator array.
|				3/25- Added rest of definitions for BB and GAVR interrupts, correct
|					  definitions for TQFP-44 package. Added all GPIO lines to GAVR
|					  and BeagleBone.
|================================================================================
| *NOTES:
\*******************************************************************************/


/* ---------------------------------------------------------------
				Interface Connector Declarations
   --------------------------------------------------------------- */

/*******************************************/
/**		BeagleBone and GAVR Interrupts	  **/
/*******************************************/
#define prtBONEINT		PORTA
#define ddrBONEINT		DDRA
#define bnBONEINT		1

#define prtGAVRINT		PORTB
#define ddrGAVRINT		DDRB
#define bnGAVRINT		3

/*******************************************/
/**				Debug LEDs				  **/
/*******************************************/
#define prtSLEEPled		PORTD
#define ddrSLEEPled		DDRD
#define pinSLEEPled		PIND
#define bnSLEEPled		7

#define prtSTATUSled	PORTC
#define ddrSTATUSled	DDRC
#define pinSTATUSled	PINC
#define bnSTATUSled		2

/*******************************************/
/**		BeagleBone and GAVR Alert Pins	  **/
/*******************************************/

#define prtInterrupts	PORTA
#define ddrInterrupts	DDRA
#define bnBBint			2
#define bnGAVRint		3

/*******************************************/
/**			Power Pin Definitions		  **/
/*******************************************/
#define prtENABLE	PORTA
#define ddrENABLE	DDRA
#define bnBBen		4
#define bnLCDen		5
#define bnGAVRen	6
#define bnGPSen		7
//Temperature
#define prtTEMPen	PORTB
#define ddrTEMPen	DDRB
#define bnTEMPen	0
//Main Regulator for WAVR
#define prtMAINen	PORTC
#define ddrMAINen	DDRC
#define bnMAINen	0

/*******************************************/
/**			GPIO Definitions			  **/
/*******************************************/
//To GAVR: bn{x}{y} where x is WAVR IO line, y is GAVR IO line
#define prtGAVRio	PORTD
#define ddrGAVRio	DDRD
#define pinGAVRio	PIND
#define bnW3G0		4
#define bnW4G1		5
#define bnW5G2		6
//To Beagle:
#define prtBBio		PORTC
#define ddrBBio		DDRC
#define pinBBio		PINC
#define bnW0B9		3
#define bnW1B10		4
#define bnW2B11		5

#define prtW6B8		PORTB
#define ddrW6B8		DDRB
#define pinW6B8		PINB
#define bnW6B8		1

/*******************************************/
/**				I2C Definitions			  **/
/*******************************************/
#define prtSDA		PORTC
#define prtSCL		PORTC
#define ddrSDA		DDRC
#define ddrSCL		DDRC
#define pinSDA		PINC
#define pinSCL		PINC
#define bnSDA		1
#define bnSCL		0

/*******************************************/
/**			UART Definitions			  **/
/*******************************************/
#define prtRXD		PORTD
#define prtTXD		PORTD
#define ddrRXD		DDRD
#define ddrTXD		DDRD
#define pinRXD		PIND
#define pinTXD		PIND
#define bnRXD		0
#define bnTXD		1

/*******************************************/
/**					INT2				  **/
/*******************************************/ 
#define prtINT2		PORTB
#define ddrINT2		DDRB
#define pinINT2		PINB
#define bnINT2		2

/*******************************************/
/**			SPI Definitions				  **/
/*******************************************/
#define	prtSpi0		PORTB
#define	pinSpi0		PINB
#define	ddrSpi0		DDRB
#define	bnSS0		4
#define	bnMosi0		5
#define	bnMiso0		6
#define	bnSck0		7

/*******************************************/
/**			SPI_1 Definitions			  **/
/*******************************************/
#define prtSpi1		PORTD
#define ddrSpi1		DDRD
#define pinSpi1		PIND
#define bnSS1		5
#define bnSCK1		4
#define bnMOSI1		3	 
#define bnMISO1		2	


/*******************************************/
/**			ADC Definitions				  **/
/*******************************************/
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