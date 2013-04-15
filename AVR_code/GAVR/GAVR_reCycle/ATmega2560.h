/*******************************************************************************\
| ATmega2560.h
| Author: Todd Sukolsky
| ID: U50387016
| Initial Build: 3/23/2013
| Last Revised: 3/25/2013
| Copyright of Todd Sukolsky and Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description:	This header contains pin declarations for the ATmega2560/1260 family
|--------------------------------------------------------------------------------
| Revisions:	3/23- Initial Build
|				3/25- Added rest of line declarations for GAVR.
|================================================================================
| *NOTES:
\*******************************************************************************/


/* ---------------------------------------------------------------
				Interface Connector Declarations
   --------------------------------------------------------------- */

/*******************************************/
/**				Debug LEDs				  **/
/*******************************************/
#define prtDEBUGled PORTL
#define ddrDEBUGled DDRL
#define bnDBG0	7
#define bnDBG1	6
#define bnDBG2	5
#define bnDBG3	4
#define bnDBG4	3
#define bnDBG5	2
#define bnDBG6	1
#define bnDBG7	0

#define prtDEBUGled2 PORTB
#define ddrDEBUGled2 DDRB
#define bnDBG8	7
#define bnDBG9	6
#define bnDBG10	5

/*******************************************/
/**				LCD Data Lines			  **/
/*******************************************/
//Databits 0-7
#define prtLCDdb0_7	PORTA
#define ddrLCDdb0_7	DDRA
#define bnLCDdb0	0
#define bnLCDdb1	1
#define bnLCDdb2	2
#define bnLCDdb3	3
#define bnLCDdb4	4
#define bnLCDdb5	5
#define bnLCDdb6	6
#define bnLCDdb7	7
//Databits 8-15
#define prtLCDdb8_15	PORTC
#define ddrLCDdb8_15	DDRC
#define bnLCDdb15	0
#define bnLCDdb14	1
#define bnLCDdb13	2
#define bnLCDdb12	3
#define bnLCDdb11	4
#define bnLCDdb10	5
#define bnLCDdb9	6
#define bnLCDdb8	7
//Touch response
#define prtLCDtouch	PORTF
#define ddrLCDtouch DDRF
#define bnLCDxr		3
#define bnLCDxl		1
#define bnLCDyt		0
#define bnLCDyb		2
//Digital Logic
#define prtLCDlogic	PORTJ
#define ddrLCDlogic DDRJ
#define bnLCDds		7
#define bnLCDwr		6
#define bnLCDrd		5
#define bnLCDcs		4
#define bnLCDreset	3
#define bnLCDblen	2

/*******************************************/
/**					GPIOs				  **/
/*******************************************/
#define prtWAVRio	PORTK
#define ddrWAVRio	DDRK
#define bnG2W5		2
#define bnG1W4		1
#define bnG0W3		0

#define prtBBio0	PORTE
#define ddrBBio0	DDRE
#define bnG3BB12	5
#define bnG4BB13	4
#define bnG5BB14	3
#define bnG6BB15	2

#define prtBBio1	PORTH
#define ddrBBio1	DDRH
#define bnG7BB16	6
#define bnG8BB17	5
#define bnG9BB18	4
#define bnG10BB19	3
#define bnG11BB20	2

/*******************************************/
/**		Interrupts to WAVR/BB			  **/
/*******************************************/
#define prtBONEINT	PORTK
#define ddrBONEINT	DDRK
#define bnBONEINT	6

#define prtWAVRINT	PORTK
#define ddrWAVRINT	DDRK
#define bnWAVRINT	7

/*******************************************/
/**					Sensors				  **/
/*******************************************/
//ADC12
#define prtHRADC	PORTK
#define ddrHRADC	DDRK
#define bnHRADC		4

//INT6
#define prtSPEEDINT	PORTE
#define ddrSPEEDINT	DDRE
#define bnSPEEDINT	6		



/*******************************************/
/**			Reception Ints				  **/
/*******************************************/
//INT7
#define prtSHUTDOWNINT	PORTE
#define ddrSHUTDOWNINT	DDRE
#define bnSHUTDOWNINT	7


#define prtRECEPTIONINT	PORTD
#define ddrRECEPTIONINT DDRD
#define bnBBReceive		1		//INT1
#define bnWAVRReceive	0		//INT0

#define BIT0			0
#define BIT1			1
#define BIT2			2
#define BIT3			3
#define BIT4			4
#define BIT5			5
#define BIT6			6
#define BIT7			7
