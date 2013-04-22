/*******************************************************************************\
| GAVR_reCycle.cpp
| Author: Todd Sukolsky
| Initial Build: 2/12/2013
| Last Revised: 4/18/13
| Copyright of Todd Sukolsky and Boston University ECE Senior Design Team Re.Cycle, 2013
|================================================================================
| Description: This is the main .cpp for the Graphics AVR for the Bike Computer
|			System. The initial implementation only had a real-time clock and is
|			being built up to final release where it will drive an LCD screen, 
|			measure HR using a touch circuit, measure speed with a reed switch
|			, and keep track of Trips. For more information and code for development
|			modules contact Todd Sukolsky at tsukolsky@gmail.com.
|--------------------------------------------------------------------------------
| Revisions:2/12- Initial Build. Demoing on ATmega644PA. Final implementation is ATmega2560
|			2/xx- Integrated RTC, started wtih startup procedures. Tried to get
|				  communication from WAVR working, not quite right. Did get simple
|				  back and forth talking with BeagleBone working.
|			3/4-  Implemented state machine to receive strings from WAVR, next is 
|				  to get communication with Bone working and Trip storage management.
|				  Moved all UART transmissions and receive protocols to an external
|				  file "myUart.h".
|			3/14- Checked over GAVR->WAVR transmission in myUart.h, looks good. Just
|				  need to field test now. Small tweaks possibly in main to make code look better.
|			3/27- Started finalizing communication protocols and integration. LCD code
|				  not yet working, so just getting everything ready. Fixed flags to work with WAVR
|				  correctly, made comments for flags and switched small logic for getting the user
|				  date and time (see main program loop). Deleted some functions for receive/send that
|				  were VERY wrong, switcehd to ATmega2560 registers and initializations. All uart
|				  transmission/reception in "myUart.h" now. (2)Added revision 1. from Revisions Needed,
|			      instituted timeout procedure for GAVR->WAVR protocol dependent on flagSendWAVR.
|			      Need to check logic on what flags are raised and the order that things happen; after
|				  check is completed need to institute definate startup procedure with heartrate code
|				  and odometer code (aka TRIP DATA).
|			3/28- Looked over SendWAVR routine and functionality in main, switched logic. Now depends
|				  on two flags to activiate, flagGetWAVRtime and flagSendWAVRtime. The flagSendWAVR
|				  if only used while sending to keep sending and check for a timeout. This allows
|				  "Revisions Needed (1)" to be irrelevant and logic is much easier to usnerstand. Now
|				  working on EEPROM trip logic.
|			4/2-  Started integrating everything into PCB module. Integrated Speed and HR, got compilation.
|				  Going to test pcbTest kill workings, then test WAVR sending ability, then WAVR->GAVR.
|			4/3-  Started working on commuincation with WAVR. Interrupt reads, not goign into the receive
|				  function thoguh meaning the WAVR isn't getting our ACKW.
|			4/5-4/6- Working on UART cleaning/transmission protocols. See "../../WAVR/WAVR/WAVR/myUart.h" and 
|				  "../../WAVR/WAVR/WAVR/WAVR.cpp" for more information. Not going to double the comments.
|			4/7- Added more flags for LCD input and a polling if statement for the LCDinput flag. Groundwork for
|				  interfacing with it. Third block of flags for more information.
|			4/8- All UART tranmissions except receiveBone work. Changed around Receive/Send if blocks/polls in
|				 main loop to allow for specific instructions to take place. Works. (2) Implemented delete functions
|				 and other trip functionality things. Need to create an environment to test things. Outlying C program
|				 is going to control what goes on when. Starts scripts in the beginning with fork() and then does them.
|				 If an interrupt occurs, forks() and then does that work there. Implemented a "vector" class myVector.
|			4/14- Testing the myVector class on x86 platform. Initialized all flags, added some cases for LCDinput/user 
|			      interaction.
|			4/15- Added functinoalty for UART2 Receiving Bone to work. Added interrupt vectors for INT5 and UART2_RX, 
|				  two flags (flagWaitingForBone2 and flagReceiveBone2), timeout capability, main loop call. Also
|				  added functionality to deleteGAVRtrip protocol. Need to delete the GPS data on the bone, changed 
|				  SendBone tactics in myUart.h. Added protocol in routine in main. 
|			4/18- Finalized all PrintBone2 functionality. Fixed NAN's in trip. AverageSpeed needs to be fixed, very wrong
|================================================================================
| Revisions Needed:
|			(1)3/27-- For timeouts on sending procedures (SendWAVR, SendBone), if a timeout
|				  occurs the flag is killed in the thing, so a subsequent send is impossible.
|				  Need to add flags that a teimtout occurred and we should try and send again? 
|				  Could be global and at end of main while check, if high, raise normal flags.
|				  ------->Revision completeted at 10:52PM, 3/27/13
|			(2)4/7--- Turn all EEPROM trip functionality into the class options. Eliminates where it is
|				  in the file and makes code much cleaner. This can be done after succsesful testing.
|================================================================================
| *NOTES: (1) This document is stored in the GIT repo @ https://github.com/tsukolsky/SeniorDesign.git/AVR_CODE/GAVR.
|			  Development is also going on in that same repo ../HR_TEST for heart
|			  rate sensing and speed detection using a reed swithc. Trip features
|			  are also being tested in that module.
|		  (2) "Waiting" timeouts can be dealt with in the main program loop, not in interrupt,
|			  since they will be going through them anyways.
|		  (3) Currently, if there is an invalid date&time the GAVR asks the WAVR
|			  for the correct time, not the user. The watchdog should always answer
|			  with the time that it has, then if the user changes it the WAVR iwll
|			  be updated with that time if it is valid. If it's not valid, then we should
|			  ask for the time from the GAVR. 
|		  (4) Bone should send trips in decrementing number, aka if it has 5 trips on it's stick,
|		      it should send trip5, trip4, etc... trip1. That way the GAVR can just show the trips
|		      in the correct order.
|		  (5) PINH6 serves as USB inserted or not GPIO. If low, no USB. if High, USB is inserted.
|		  (6) Routine ShowUSBTrips is somewhat repetitive/stuipd. It would be smarter to make "nice" strings immidiately
|			  when USB strings come in, or immediately after, however this works as well. One idea is to call a function
|			  after ReceiveBone is called and flagHaveUSBTrips is high and move those stings into a nicer form in a different vector.
|===============================================================================
| Debug LED Configuration: Top-Bottom=LED13->LED20, LED10->12
|									  DBG0->DBG7,   DBG8->10
|		DBG0=flagQuit
|		DBG1=SpeedInterrupt
|		DBG2=SendWAVR
|		DBG3=ReceiveBone2
|		DBG4=InterruptFromWAVR
|		DBG5=SendBone
|		DBG6=<>
|		DBG7=flagUSBinserted
|		DBG8=ReceiveBone
|		DBG9=ReceiveWAVR
|		DBG10=RTCosc
\*******************************************************************************/

using namespace std;
//Whether or not this will ask to communicate with WAVR after a startup timeout. Comment out if you want to enable the timeout
#define TESTING

#include <inttypes.h>
#include <math.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/eeprom.h>
#include "stdtypes.h"		//holds standard type definitions
#include "myTime.h"			//myTime class, includes the myDate.h inherently
#include "trip.h"
#include "eepromSaveRead.h"	//includes save and read functions as well as checking function called in ISR
#include "ATmega2560.h"
#include "myVector.h"
#include "gavrUart.h"



/*****************************************/
/**			UART Frequency/BAUD			**/
/*****************************************/
#define FOSC 8000000
#define BAUD 9600
#define MYUBRR FOSC/16/BAUD -1	//declares Baud Rate for UART

/*****************************************/
/**			TIMEOUT DEFINITIONS			**/
/*****************************************/
#define COMM_TIMEOUT_SEC 8
#define STARTUP_TIMEOUT_SEC 120

/*****************************************/
/**			Interrupt Macros			**/
/*****************************************/
#define __killCommINT() EIMSK &= ~((1 << INT0)|(1 << INT1)|(1 << INT5))		//disable any interrupt that might screw up reception. Keep the clock ticks going though
#define __enableCommINT()  EIMSK |= (1 << INT0)|(1 << INT1)|(1 << INT5)
#define __killSpeedINT() EIMSK &= ~(1 << INT6)
#define __enableSpeedINT() EIMSK |= (1 << INT6)

/*****************************************/
/**			Speed and HR Defs			**/
/*****************************************/
#define TIMER1_OFFSET		65535
#define BAD_SPEED_THRESH	3
#define CALC_SPEED_THRESH	3				//at 20MPH, 4 interrupts take 1 second w/clk, should be at least every second
#define MINIMUM_HR_THRESH	350				//
#define MAXIMUM_HR_THRESH	800				//seen in testing it usually doesn't go more than 100 above or below 512 ~ 1.67V
#define NUM_MS				4				//4 ms
#define ONE_MS				0x20			//32 ticks on 8MHz/256 is 1ms

/*****************************************/
/**			Foward Declarations			**/
/*****************************************/
void DeviceInit();
void AppInit(unsigned int ubrr);
void EnableRTCTimer();
void Wait_sec(unsigned int delay);
void initHRSensing();
void initSpeedSensing();
WORD GetADC();

/*********************************************GLOBAL FLAGS*******************************************************/
/****************************************************************************************************************/
/*==============================================================================================================*/
BOOL justStarted, flagQUIT, flagGetUserClock, flagWAVRtime, flagGetWAVRtime, flagInvalidDateTime, flagUpdateWAVRtime;
/* justStarted: Whether or not the chip just started up. If so, there is a startup timeout and procedure with	*/
/*				flags that goes on to accurately set the Time and Date											*/
/* flagQUIT: We are about to get shut down, need to quick save everything in EEPROM and then just go into		*/
/*			 down mode.																							*/
/* flagWAVRtime: Whether or not we are using time that was sent from the WAVR or time that we got from the user.*/
/* flagGetUserClock: WAVR needs the time (GPS not valid), need to get it from the user							*/
/* flagGetWAVRtime: Get the time from the WAVR, not really sure when this would happen...maybe an error.		*/
/* flagInvalidDateTime: It's been detected that the time/date in the system is invalid...						*/	
/* flagUpdateWAVRtime: We got the User Clock, time to update the WAVR.											*/														
/*==============================================================================================================*/

/*==============================================================================================================*/
BOOL flagReceiveWAVR, flagSendWAVR, flagReceiveBone, flagReceiveBone2, flagSendBone, flagWaitingForWAVR, flagWaitingForBone, flagWaitingForBone2, flagNewTripStartup;
/* flagReceiveWAVR: We are receiving, or about to receive, transmission from WAVR								*/
/* flagSendWAVR: Sending to WAVR, or about to. We need to ask the WAVR for date/time or send it the date and	*/
/*				  time it asked for.																			*/
/* flagReceiveBone: Receiving, or about to receive, the transmission from the BeagleBone on UART0				*/
/* flagReceiveBone2: Receiving, or about to receive, the transmission from beagleBone on UART2					*/
/* flagSendBone: Sending to, or about to send, information to the Bone											*/
/* flagWaitingForWAVR: Waiting to receive a UART string from WAVR, let main program know that with this flag.	*/
/* flagWaitingForBone: Waiting to receive a UART string from the BONE on uart0 this alerts main program.		*/
/* flagWaitingForBone2: Waiting to receive a UART string from Bone on UART2.									*/
/* flagNewTripStartup: Whether or not a new trip was created on startup, or old trip was loaded. If new trip,	*/
/*					need a date update from the WAVR before we can do anything (flag stays high until update).	*/
/*					If false, will stay false until a reboot.													*/
/*==============================================================================================================*/

/*==============================================================================================================*/
BOOL flagLCDinput, flagUSBinserted, flagUpdateWheelSize,flagTripOver, flagDeleteGAVRTrip, flagDeleteUSBTrip;
/* flagLCDinput: We are accepting input from the LCD.															*/
/* flagUSBinserted: There is a USB stick inserted in the module, poll options to see if user wants to act.		*/
/* flagUpdateWheelSize: User updated the wheel size, set the wheel size of the trip to this new one.			*/
/* flagTripOver: User just stopped the current trip, set up a new one.											*/
/* flagDeleteGAVRTrip: Deleting a trip in the GAVR EEPROM.														*/
/* flagDeleteUSBTrip: Delete a trip on the BeagleBone.															*/
/*==============================================================================================================*/

/*==============================================================================================================*/
BOOL flagHaveUSBTrips, flagOffloadTrip, flagNeedTrips, flagViewUSBTrips, flagViewGAVRtrips;
/* flagHaveUSBTrips: Whether or not there are USB trips in the USBtripsViewer vector							*/
/* flagOffloadTrip:	We are sending one of our trips to the BeagleBone.											*/
/* flagNeedTrips: User wants to see the trips, ask for them.													*/
/* flagViewUSBTrips: User wants to view USB trips.																*/
/* flagViewGAVRTrips: User wants to view GAVR trips.															*/
/*==============================================================================================================*/

/*****************************************/
/**			Global Variables			**/
/*****************************************/
myTime currentTime;					//The clock, must be global. Initiated as nothing.
trip globalTrip;					//Trip must be global as well, initialed in normal way. Startup procedure sets it. 
myVector<char *> USBtripsViewer;	//Vector to hold trip data sent over by the USB
//char *actualTrips[20];
//BYTE numberOfUSBTrips=0;
WORD HRSAMPLES[300];				//Array of HR Samples passed to the hrMonitor.h class.
WORD numberOfSpeedOverflows=0;		//How many times the counter has overflowed for the reed switch. 
BOOL flagNoSpeed=fFalse;			//Whether or not there is a default of NO speed right now.
BOOL flagShowStats=fFalse;			//Whether we should show the statistics of the biker, debug
WORD numberOfGAVRtrips=0;			//Represents how many trips are stored in EEPROM +1
int whichTripToOffload=-1, whichUSBtripToDelete=-1,whichGAVRtripToDelete=-1;
unsigned int daysInMonth[12]={31,28,31,30,31,30,31,31,30,31,30,31};

/*--------------------------Interrupt Service Routines------------------------------------------------------------------------------------*/
//ISR for beaglebone uart input
ISR(USART0_RX_vect){
	cli();
	if (flagWaitingForBone){
		flagWaitingForBone=fFalse;
		flagReceiveBone=fTrue;
	} else {
		flagReceiveBone=fFalse;
	}
	UCSR0B &= ~(1 << RXCIE0);
	sei();
}
/************************************************************************/
//ISR for WAVR uart input.
ISR(USART1_RX_vect){
	cli();
	if (flagWaitingForWAVR){
		flagWaitingForWAVR=fFalse;
		flagReceiveWAVR=fTrue;
	} else {
		flagReceiveWAVR=fFalse;
	}
	UCSR1B &= ~(1 << RXCIE1);	//clear interrupt. Set UART flag
	sei();
}
/************************************************************************/
ISR(USART2_RX_vect){
	/*char recChar;
	recChar=UDR2;
	PutUartChBone2(recChar);*/
	cli();
	if (flagWaitingForBone2){
		flagWaitingForBone2=fFalse;
		flagReceiveBone2=fTrue;
	} else {
		flagReceiveBone2=fFalse;
	}	
	UCSR2B &= ~(1 << RXCIE2);
	sei();
}	
/************************************************************************/
//INT0, getting something from the watchdog
ISR(INT0_vect){	// got a signal from Watchdog that time is about to be sent over.
	cli();
	prtDEBUGled|= (1 << bnDBG4);
	flagWaitingForWAVR=fTrue;
	__killCommINT();
	//Wait for UART0 signal now, otherwise do nothing
	PrintWAVR("A.");		//A for ACK
	UCSR1B |= (1 << RXCIE1);
	prtDEBUGled &= ~(1 << bnDBG4);
	sei();
}
/************************************************************************/
//INT1, getting something from the BeagleBone
ISR(INT1_vect){
	cli();
	flagWaitingForBone=fTrue;
	UCSR0B |= (1 << RXCIE0);	//enable receiver 1
	__killCommINT();
	PrintBone("A.");			//Ack from GAVR
	sei();
}
/************************************************************************/
ISR(INT5_vect){
	cli();
	flagWaitingForBone2=fTrue;
	UCSR2B |= (1 << RXCIE2);
	__killCommINT();
	PrintBone2("A.");
	sei();
}
/************************************************************************/

//INT6-> Speed magnet went around.
ISR(INT6_vect){
	//Do something with speed/odometer/trip thing.
	cli();
	WORD value=TCNT1;
	if (flagNoSpeed){
		flagNoSpeed=fFalse;
		globalTrip.resetSpeedPoints();
	}//end if no speed.
	if (((value > 3000 && numberOfSpeedOverflows==0)|| numberOfSpeedOverflows>0) && !flagNoSpeed){
		prtDEBUGled |= (1 << bnDBG1);
		globalTrip.addSpeedDataPoint(value+numberOfSpeedOverflows*TIMER1_OFFSET);
		numberOfSpeedOverflows=0;	
		prtDEBUGled &= ~(1 << bnDBG1);
	} else;		//was a repeat/low value, not valid. Go forward.
	TCNT1=0x00;
	sei();
}
/************************************************************************/
//INT7->IMMININENT SHUTDOWN
ISR(INT7_vect){
	cli();
	if (flagQUIT){flagQUIT=fFalse;}
	else{flagQUIT=fTrue;}
	sei();
}
/************************************************************************/
ISR(TIMER0_COMPA_vect){
	cli(); 
	static BOOL flagGettingHR=fFalse;
	static WORD location=0;
	//Take a reading every 10 seconds.
	if (currentTime.getSeconds()%10==0 && !flagGettingHR){
		flagGettingHR=fTrue;
	} 
	if (flagGettingHR){
		//Declare variables
		WORD signal=0;
		//Increment N (time should reflect number of ms between timer interrupts), get ADC reading, see if newSample is good for anything.
		signal = GetADC();		//retrieves ADC reading on ADC0
		HRSAMPLES[location++]=signal;
		if (location >= 300){location=0; globalTrip.calculateHR(HRSAMPLES, 300); flagGettingHR=fFalse;}
	}//end if getting HR.
	//Re-enable interrupts
	sei();
}
/************************************************************************/
//Timer for Speed.
ISR(TIMER1_OVF_vect){
	cli();
	if (numberOfSpeedOverflows++ > BAD_SPEED_THRESH && !flagNoSpeed){
		//Let the INT6 know that on next interrupt it shouldn't calc speed, but initialize "speedPoints" in odometer class.
		flagNoSpeed=fTrue;
		globalTrip.resetSpeedPoints();					//repetitive, done in the interrupt vector? Needs to be done twice just in case the wheel stops spinning.
	}
	sei();
}

/************************************************************************/
ISR(TIMER2_OVF_vect){
	volatile static int WAVRtimeout=0, BONEtimeout=0, startupTimeout=0, sendingWAVRtimeout=0, BONE2timeout=0, sendingBoneTimeout=0;
	prtDEBUGled2 ^= (1 << bnDBG10);
	
	if (currentTime.getSeconds()%10==0){flagShowStats=fTrue;}
	//volatile static int timeOut = 0;
	currentTime.addSeconds(1);
	globalTrip.addTripSecond();
	//Timeout functionality
			
	//UART0/Bone Trips Send timeout
	if (BONEtimeout <= COMM_TIMEOUT_SEC && (flagReceiveBone || flagWaitingForBone)){BONEtimeout++;}	
	else if (BONEtimeout > COMM_TIMEOUT_SEC && (flagReceiveBone || flagWaitingForBone)){BONEtimeout=0; flagReceiveBone=fFalse; flagWaitingForBone=fFalse; __enableCommINT();}
	else if (!flagReceiveBone && !flagWaitingForBone && BONEtimeout >0){BONEtimeout=0;}
	else;
	
	//UART0/Sending to Bone timeout
	if (flagSendBone && sendingBoneTimeout <= COMM_TIMEOUT_SEC*2){sendingBoneTimeout++;}
	else if (flagSendBone && sendingBoneTimeout > COMM_TIMEOUT_SEC*2){sendingBoneTimeout=0;flagSendBone=fFalse; __enableCommINT();}		//this doesn't allow for a resend.
	else if (!flagSendBone && sendingBoneTimeout > 0){sendingBoneTimeout=0;}
	else;	
	
	//UART2/Bone Debug Tiemtout
	if (BONE2timeout <= COMM_TIMEOUT_SEC && (flagReceiveBone2 || flagWaitingForBone2)){BONE2timeout++;}
	else if (BONE2timeout > COMM_TIMEOUT_SEC && (flagReceiveBone2 || flagWaitingForBone2)){BONE2timeout=0; flagReceiveBone2=fFalse; flagWaitingForBone2=fFalse; __enableCommINT();}
	else if (!flagReceiveBone2 && !flagWaitingForBone2 && BONE2timeout >0){BONE2timeout=0;}
	else;
	
	//If sending to WAVR, institue the timeout
	if (flagSendWAVR && sendingWAVRtimeout <= COMM_TIMEOUT_SEC){sendingWAVRtimeout++;}
	else if (flagSendWAVR && sendingWAVRtimeout > COMM_TIMEOUT_SEC){sendingWAVRtimeout=0;flagSendWAVR=fFalse; __enableCommINT();}		//this doesn't allow for a resend.
	else if (!flagSendWAVR && sendingWAVRtimeout > 0){sendingWAVRtimeout=0;}
	else;
	
	//If WAVR receive timout is reached, either waiting or actually sending, reset the timeout, bring flags down and enable interrupt pins again
	if (WAVRtimeout <= COMM_TIMEOUT_SEC && (flagReceiveWAVR || flagWaitingForWAVR)){WAVRtimeout++;}
	else if ((WAVRtimeout > COMM_TIMEOUT_SEC) && (flagReceiveWAVR || flagWaitingForWAVR)){WAVRtimeout=0; flagReceiveWAVR=fFalse; flagWaitingForWAVR=fFalse; __enableCommINT();}
	else if (!flagReceiveWAVR && !flagWaitingForWAVR && WAVRtimeout > 0){WAVRtimeout=0;}	//Both flags aren't set, make sure the timeout is 0
	else;

#ifndef TESTING		
	//If we just started, increment the startUpTimeout value. When it hits 30, logic goes into place for user to enter time and date
	if (justStarted && startupTimeout <= STARTUP_TIMEOUT_SEC){startupTimeout++;}
	else if (justStarted && startupTimeout > STARTUP_TIMEOUT_SEC){
		startupTimeout=0; 
		justStarted=fFalse;
		if (!flagWAVRtime){flagGetWAVRtime=fTrue;}				//if we haven't gotten the date and time from WAVR yet, say that we need it.
		else {flagGetWAVRtime=fFalse;}							//make sure it's low. not really necessary.
	}	
	else if (!justStarted && startupTimeout > 0){startupTimeout=0;}
	else;
#endif	
}

/*--------------------------END-Interrupt Service Routines--------------------------------------------------------------------------------*/
/*--------------------------START-Main Program--------------------------------------------------------------------------------------------*/


int main(void)
{
	DeviceInit();		//Sets all ports and ddrs to something
	AppInit(MYUBRR);	//initializes pins for our setting
	EnableRTCTimer();	//RTC
	getDateTime_eeprom(fTrue,fTrue);	//Get last saved date and time.
	initSpeedSensing();
//	initHRSensing();
	//Check the trip datas to see if there is something going on.
	flagNewTripStartup=StartupTripEEPROM();		//If a new trip is needed, calls it. Otherwise is loads a new one. True means a new one started.
	Wait_sec(2);
	if (flagNewTripStartup){PrintBone2("Resuming an old trip");}
	else {PrintBone2("Starting a new trip");}
	sei();
    while(fTrue)
    {	
		//Receiving from WAVR. Either a time string or asking user to set date/time/both.
		if (flagReceiveWAVR && !flagQUIT && !flagReceiveBone){
			prtDEBUGled2 |= (1 << bnDBG9);		//second from bottom, next to RTC...
			ReceiveWAVR();
			Wait_sec(2);
			if (!flagReceiveBone && !flagWaitingForBone && !flagReceiveBone2 && !flagWaitingForBone2){		//re enable comm interrupts only if we aren't waiting for one currently, or are about to go into one.
				__enableCommINT();
			}
			prtDEBUGled2 &= ~(1 << bnDBG9);
		}

		//Receiving from the Bone. Bone has priority over WAVR
		if (flagReceiveBone && !flagQUIT){
			prtDEBUGled2 |= (1 << bnDBG8);		//third from bottom.
			PrintBone2("Receiving from bone.");
			ReceiveBone();
			Wait_sec(2);
			if (!flagReceiveWAVR && !flagWaitingForWAVR && !flagReceiveBone2 && !flagWaitingForBone2){
				__enableCommINT();
			}
			prtDEBUGled2 &= ~(1 << bnDBG8);	
		}		
		
		//If we are supposed to be getting something from the Bone's debug UART. WAVR and UART0 Bone have priority. Lowest on the totem pole.
		if (flagReceiveBone2 && !flagQUIT && !flagReceiveBone && !flagReceiveWAVR){
			prtDEBUGled |= (1 << bnDBG3);
			ReceiveBone2();
			Wait_sec(10);
			if (!flagReceiveWAVR && !flagWaitingForWAVR){
				__enableCommINT();
			}
			prtDEBUGled &= ~(1 << bnDBG3);
		}

		//Send either "I need the date or time" or "Here is the date and time you asked for. If we are still waiting for WAVR, then don't send anything.
		if ((flagGetWAVRtime || flagUpdateWAVRtime) && !flagWaitingForWAVR && !flagQUIT){
			prtDEBUGled |= (1 << bnDBG2);		//third from top.
			__killCommINT();
			PrintBone2("Sending to WAVR.");
			SendWAVR();	
			Wait_sec(2);
			if (!flagReceiveWAVR && !flagWaitingForWAVR && !flagReceiveBone && !flagWaitingForBone){
				__enableCommINT();
			}	
			prtDEBUGled &= ~(1 << bnDBG2);		
		}
		
		//WAVR hasn't updated the time, need to get it from user...make sure WAVR isn't about to send the time, or waiting to
		if (flagGetUserClock && !flagWaitingForWAVR && !flagQUIT){
			//Get the time and date from the user
			/* this is where we do that in LCD land. */
			//if time/date valid, set, save,send
			flagUpdateWAVRtime=fTrue;
			flagGetUserClock=fFalse; 
			flagWAVRtime=fFalse;
			flagGetWAVRtime=fFalse;							//SendWAVR() will set the flags down accordingly
			
		}
		
		
/**************testing procedure start********************/					
#ifdef TESTING		
		/* Debugging/PRE-LCD functionality. Prints every 10 seconds */
		if (flagShowStats && !flagWaitingForBone && !flagWaitingForWAVR && !flagReceiveBone && !flagReceiveWAVR && !flagReceiveBone2 && !flagWaitingForBone2 && !flagQUIT){
			cli();
			flagShowStats=fFalse;
			//Put together a time string.
			char timeString[40];
			strcpy(timeString,currentTime.getTime());
			strcat(timeString,"/");
			strcat(timeString,currentTime.getDate());
			strcat(timeString,".\0");
			//Get other values to print
			double speed=globalTrip.getCurrentSpeed();
			double aveSpeed=globalTrip.getAverageSpeed();
			double distance=globalTrip.getDistance();
			double currentHR=globalTrip.getCurrentHR();
			double averageHR=globalTrip.getAveHR();
			WORD sdays=globalTrip.getStartDays();
			WORD syear=globalTrip.getStartYear();
			WORD minElapsed= globalTrip.getMinutesElapsed();
			WORD dayElapsed=globalTrip.getDaysElapsed();
			WORD yearElapsed=globalTrip.getYearsElapsed();
			char speedString[8],aveSpeedString[8],distanceString[8],currentHRstring[8],aveHRstring[8], startString[9],yearString[5], timeLapseString[10];
			//Turn doubles and ints into strings.
			dtostrf(speed,5,3,speedString);
			dtostrf(aveSpeed,5,3,aveSpeedString);
			dtostrf(distance,7,4,distanceString);
			dtostrf(currentHR,4,2,currentHRstring);
			dtostrf(averageHR,4,2,aveHRstring);
			//make start string
			utoa(sdays,startString,10);
			utoa(syear,yearString,10);
			strcat(startString,"/");
			strcat(startString,yearString);
			//make timeLapseString
			utoa(minElapsed,timeLapseString,10);
			char dayStr[4],yearStr[5];
			utoa(dayElapsed,dayStr,10);
			utoa(yearElapsed,yearStr,10);
			strcat(timeLapseString,":");
			strcat(timeLapseString,dayStr);
			strcat(timeLapseString,":");
			strcat(timeLapseString,yearStr);
			char howManyString[4];
			utoa(numberOfGAVRtrips,howManyString,10);
			char viewSize[4];
			utoa(USBtripsViewer.size(),viewSize,10);
			//Print them to Bone2 port (BONE RX5)
			PrintBone2("***");
			PrintBone2("Trip viewer size=");
			PrintBone2(viewSize);
			PrintBone2("---Number of GAVR Trips:");
			PrintBone2(howManyString);
			PrintBone2("---Time/Date:");
			PrintBone2(timeString);
			PrintBone2("--Start Days/Year");
			PrintBone2(startString);
			PrintBone2("--Time lapsed:");
			PrintBone2(timeLapseString);
			PrintBone2("--Sp:");
			PrintBone2(speedString);
			PrintBone2("--AvSp:");
			PrintBone2(aveSpeedString);
			PrintBone2("--Di:");
			PrintBone2(distanceString);
			PrintBone2("--HR:");
			PrintBone2(currentHRstring);
			PrintBone2("--AvHR:");
			PrintBone2(aveHRstring);
			PrintBone2("*** ");
			flagShowStats=fFalse;
			sei();
		}
		
		//Here is where we react to what the LCD gets as inputs.		
		if (flagLCDinput){
			//User changed the wheel size
			if (flagUpdateWheelSize){
				double tempWheelSize=-2;
				globalTrip.setWheelSize(tempWheelSize);
			}				
			
			//If User ends a trip, start a new one while saving everything in EEPROM...Testing, good I think.
			if (flagTripOver){						//If the trip is over, end this one then start a new one.
				static BOOL continueWithNew=fTrue;
				PrintBone2("In flagTripOver.");
				if (continueWithNew){
					EndTripEEPROM();
					StartNewTripEEPROM();
				}
				prtDEBUGled |= (1 << bnDBG5);
				SendBone(0);						//Tell the bone a new trip was started.
				prtDEBUGled &= ~(1 << bnDBG5);
				//Flag is reset in SendBone section.
				if (flagTripOver){
					continueWithNew=fFalse;
				} else {
					continueWithNew=fTrue;
				}
			}		
				
			//If we need to delete a trip from the GAVR, delete it and then tell bone to delete that GPS file.
			if (flagDeleteGAVRTrip){
				PrintBone2("Deleting GAVR trip.");
				BYTE trips=eeprom_read_byte(&eeNumberOfTrips);		//see how many trips there are
				if (numberOfGAVRtrips!=trips){
					numberOfGAVRtrips=trips;
				}
				static BOOL continueWithDelete=fTrue;
				//Make it so we can't delete the current trip.
				if (whichGAVRtripToDelete==numberOfGAVRtrips){
					flagDeleteGAVRTrip=fFalse;
					continueWithDelete=fFalse;
				}
				//delete the trip
				if (continueWithDelete){
					PrintBone2("Deleting EEPROM...");
					DeleteTrip(whichGAVRtripToDelete);
				}
				prtDEBUGled |= (1 << bnDBG5);
				SendBone(whichGAVRtripToDelete);					//Will leave flag high if there was an error. If there was, don't do anything
				prtDEBUGled &= ~(1 << bnDBG5);
				if (flagDeleteGAVRTrip){
					PrintBone2("Unsuccessful delete on Bone.");
					//Flag is still high, don't delete on next round
					continueWithDelete=fFalse;
				} else {
					PrintBone2("Successful delete on Bone.");
					//successfully deleted GPS data from the BeagleBone. Next time this is called it will be for a new trip. Good to go.
					continueWithDelete=fTrue;
					whichGAVRtripToDelete=-1;
				}	
				
			}//end if flagDeleteGAVRTrip
			
			//If user wants to see the trips on the GAVR, show them.
			if (flagViewGAVRtrips){
				PrintBone2("In viewGAVR trips.");
				BYTE trips=eeprom_read_byte(&eeNumberOfTrips);		//see how many trips there are
				if (numberOfGAVRtrips!=trips){
					numberOfGAVRtrips=trips;
				}
				if (numberOfGAVRtrips>1){
					char tempNum[5];
					utoa(numberOfGAVRtrips,tempNum,10);
					PrintBone2(tempNum);
					PutUartChBone2('-');
					//Declare variables for capture in HEAP memory. Start Day, Start Year, strings.
					WORD tripDays,tripMonth=1,tripYear=2013;
					myVector<char *> tripIDs;
					unsigned int offset;
					for (int i=0; i<numberOfGAVRtrips-1;i++){
						tripMonth=1;
						offset=i*BLOCK_SIZE+INITIAL_OFFSET;
						tripDays=eeprom_read_word((WORD *)(offset+START_DAYS));
						tripYear=eeprom_read_word((WORD *)(offset+START_YEAR));
						//Get what month it is, also days.
						while (tripDays>daysInMonth[tripMonth-1]){
							tripDays-=daysInMonth[tripMonth-1];
							tripMonth++;
						}
						if (tripDays>28 && tripMonth==2){
							tripDays-=daysInMonth[tripMonth-1];
							tripMonth++;
						}
						//Assemble the strings. Define array of stack strings
						char tempString[5],tempString1[5],tempString2[5],tempString3[40];
						utoa(tripMonth,tempString,10);
						utoa(tripDays,tempString1,10);
						utoa(tripYear,tempString2,10);

						//Put together the tempString
						strcpy(tempString3,"Trip ");
						strcat(tempString3,tempString);
						strcat(tempString3,"/");
						strcat(tempString3,tempString1);
						strcat(tempString3,"/");
						strcat(tempString3,tempString2);
						//String complete, push onto the vector
						tripIDs.push_back(tempString3);
					}
					//Print Those trips
					for (int j=0;j<tripIDs.size();j++){
						PrintBone2(tripIDs[j]);
					}			
				} else {PrintBone2("Only one trip.");}
				flagViewGAVRtrips=fFalse;
			}//end flagViewGAVRtrips									
			//If there is a USB and user asks to delete trips, offload a trip, or view trips on the USB, tell the Bone.
			if (flagUSBinserted){
				//We want to view the trips, get them and push them onto a template stack.? or vector push?->Only looking at start date in first implementation(use char vector).
				if (flagNeedTrips){
					flagHaveUSBTrips=fFalse;
					prtDEBUGled |= (1 << bnDBG5);
					PrintBone2("Asking for trips.");
					SendBone(0);
					prtDEBUGled &= ~(1 << bnDBG5);
					if (flagNeedTrips){PrintBone2("Unsuccessful ask for trips.");}
				}
				//Get which trip the user wants to offload and send it to the Bone
				if (flagOffloadTrip && !flagDeleteUSBTrip){
					//static BOOL okToTry=fTrue;
					//static BYTE okToTryTime=(currentTime.getSeconds()+10)%60;
					PrintBone2("Offloading trip.");
					prtDEBUGled |= (1 << bnDBG5);
					SendBone(whichTripToOffload);
					prtDEBUGled &= ~(1 << bnDBG5);
				/*	if (flagOffloadTrip){
						if (!okToTry && currentTime.getSeconds()==okToTryTime){
							okToTry=fTrue;	
						} else if (!okToTry && currentTime.getSeconds()!=okToTryTime){
							okToTry=fFalse;
						} else;
					}//end if offload was unsuccessful, should wait for a few seconds.*/
				//Tell the beaglebone to delete one of it's trips.
				} else if (flagDeleteUSBTrip && !flagOffloadTrip){
					myVector<char *> USBtripsHolder;
					
					//tell the bone to delete the trip.
					SendBone(whichUSBtripToDelete);
					
					//Delete that trip from the USBtripsViewer
					BYTE numberOfUSBTrips=USBtripsViewer.size();
					
					//If there are 4 trips and we want to delete trip two, we pop trips 4->3 into new buffer, then pop oen to delete, then push 3->4 back on (now 2->3).
					BYTE numberOfPops=numberOfUSBTrips-whichUSBtripToDelete;
					for (int i=0; i<numberOfPops;i++){
						char *tempString;
						strcpy(tempString,USBtripsViewer.pop_back());
						USBtripsHolder.push_back(tempString);
					}
					//Pop the one to delete off
					char *tString;
					strcpy(tString,USBtripsViewer.pop_back());
					
					//Push them back onto the USBtripsViewer.
					for (int i=0; i<numberOfPops; i++){
						char *tempString;
						strcpy(tempString,USBtripsHolder.pop_back());
						USBtripsViewer.push_back(tempString);
					}
				} else;//end if flagDeleteUSBTrip && !flagOffloadTrip
				
				//If user wants to see the trips on the Bone, show them if we have them
				if (flagViewUSBTrips && !flagNeedTrips && flagHaveUSBTrips){
					PrintBone2("Viewing trips.");
					BOOL flagPrint=fTrue;
					int numberOfUSBTrips=USBtripsViewer.size();
					if (numberOfUSBTrips>0){
						myVector<char *> usbtripIDs;
						char *tempStrings0;
						char tempStrings1[4], tempStrings2[3],tempStrings3[5], tempStrings4[40];
						//[0]=popped from viewer, [1]=days,[2]=months,[3]=years,[4]=assembled string.
						//Parse the string and make a nicer string.
						for (int i=0; i<numberOfUSBTrips;i++){
							strcpy(tempStrings0,USBtripsViewer[i]);
							PrintBone2(USBtripsViewer[i]);
							//strcpy(tempStrings0,actualTrips[numberOfUSBTrips]);
							PrintBone2("String:");
							PrintBone2(tempStrings0);
							//Need to find what the days were. Locatoin=location in tempStrings[0], dayPlacement=tempStings[1], yearPlacement=tempStrings[3], tDays and months used for getting right value.
							int location=1, dayPlacement=0, yearPlacement=0, tripDays=1,tripMonth=1;
							
							//Get the number of days otu of the string
							while (tempStrings0[location] != '/' && tempStrings0[location] != '.' && tempStrings0[location] != '\0'){
								tempStrings1[dayPlacement++]=tempStrings0[location++];
							}//broke
							if (tempStrings0[location]=='/'){
								int yearPlacement=0;
								location++;
								while (tempStrings0[location] != '.' && tempStrings0[location] != '\0'){
									tempStrings3[yearPlacement++]=tempStrings0[location++];
								}//end while moving year	
							}//end if we found '/'
						/*	PrintBone2("Days:");
							PrintBone2(tempStrings1);
							PrintBone2("Year:");
							PrintBone2(tempStrings3);*/
							//Turn dayString into a number of days.
							if (dayPlacement>0 && yearPlacement>0){
								tripDays=atoi(tempStrings1);
								tripMonth=1;
								//Get what month it is, also days.
								while (tripDays>daysInMonth[tripMonth-1]){
									tripDays-=daysInMonth[tripMonth-1];
									tripMonth++;
								}
								if (tripDays>28 && tripMonth==2){
									tripDays-=daysInMonth[tripMonth-1];
									tripMonth++;
								}//end finding of days and month
								utoa(tripDays,tempStrings1,10);
								utoa(tripMonth,tempStrings2,10);
							/*	PrintBone2("NewDays:");
								PrintBone2(tempStrings1);
								PrintBone2("Month:");
								PrintBone2(tempStrings2);*/
								//Merge all strings into a start date in tempStrings[4]
								strcpy(tempStrings4,tempStrings2);
								strcat(tempStrings4,"/");
								strcat(tempStrings4,tempStrings1);
								strcat(tempStrings4,"/");
								strcat(tempStrings4,tempStrings3);
								//Push onto usbtripIds
								usbtripIDs.push_back(tempStrings4);	
							}//end if day and year placement>0
							else {flagPrint=fFalse;}
						}//end for i<numberOfTrips
					
						if (flagPrint){
							PrintBone2("USB Trips: ");
							for (int i=0; i<usbtripIDs.size(); i++){
								PrintBone2(usbtripIDs[i]);
								PrintBone2("...");
								flagViewUSBTrips=fFalse;
							}//end for
						} else {
							PrintBone2("Error with trip strings, resetting have bools.");
							flagHaveUSBTrips=fFalse;
							flagNeedTrips=fTrue;
							for (int i=0; i<USBtripsViewer.size(); i++){
									char *tempString=USBtripsViewer.pop_back();
									PrintBone2("Popped ");
									PrintBone2(tempString);
							}		
						}//end if flagPrint
					}else {
						PrintBone2("Don't have trips, resetting bools."); 
						flagHaveUSBTrips=fFalse;
						flagNeedTrips=fTrue;
						for (int i=0; i<USBtripsViewer.size(); i++){
							char *tempString=USBtripsViewer.pop_back();
							PrintBone2("Popped ");
							PrintBone2(tempString);
						}		//end for
						flagViewUSBTrips=fTrue;
					}	//end if (USBtripsViewer.size()>0)-else
				}//end flagViewUSBTrips
			}//end if USBinserted
		}//end LCD input	
#endif

/**************testing procedure end********************/
		
		//If shutdown is imminent, go bye-bye	
		if (flagQUIT){
			cli();
			//Save all trip date into EEPROM with "unfinished/finished" label.
			SaveTripSHUTDOWN();
			//Show that we know we are supposed to quit
			prtDEBUGled |= (1 << bnDBG0);
			Wait_ms(500);
			prtDEBUGled = 0x00;
			prtDEBUGled2 &= ~((1 << bnDBG10)|(1 << bnDBG9)|(1 << bnDBG8));
			prtWAVRio &= ~(1 << bnG0W3);		
			//Power Down
			SMCR = (1 << SM1);		//power down mode
			SMCR |= (1 << SE);		//enable sleep
			sei();
			asm volatile("SLEEP");	//go to sleep
		}		
		
		//IF we have an invalid time, ask the WAVR for a time.
		if (flagInvalidDateTime){
			flagGetWAVRtime=fTrue;
		}			
		
		//Check GPIO pin H7 to see if USB is inserted.
		if ((pinBBio1 & (1 << bnG8BB17))&& !flagUSBinserted){
			flagUSBinserted=fTrue;
			prtDEBUGled |= (1 << bnDBG7);	
		} else if (!(pinBBio1 & (1 << bnG8BB17)) && flagUSBinserted){
			flagUSBinserted=fFalse;
			prtDEBUGled &= ~(1 << bnDBG7);
		}//end if-else USB inserted.
							
    }//end while
}//end main


/*************************************************************************************************************/
void DeviceInit(){
	//Set all ports to input with no pull
	DDRB = 0;
	DDRC = 0;
	DDRD = 0;
	DDRE = 0;
	DDRF = 0;
	DDRG = 0;
	DDRH = 0;
	DDRJ = 0;
	DDRK = 0;
	DDRL = 0;

	PORTB = 0;
	PORTC = 0;
	PORTD = 0;
	PORTE = 0;
	PORTF = 0;
	PORTG = 0;
	PORTH = 0;
	PORTJ = 0;
	PORTK = 0;
	PORTL = 0;
}
/*************************************************************************************************************/
void AppInit(unsigned int ubrr){
	
	//Set BAUD rate of UART0, 1, 2
	UBRR0L = ubrr;   												//set low byte of baud rate for uart0
	UBRR0H = (ubrr >> 8);											//set high byte of baud rate for uart0
	UBRR1L = ubrr;													//set low byte of baud for uart1
	UBRR1H = (ubrr >> 8);											//set high byte of baud for uart1
	UBRR2L = ubrr;
	UBRR2H = (ubrr >> 8);	
	
	//Enable UART_TX0, UART_RX0, UART_RX1, UART_TX1
	UCSR0B = (1 << TXEN0)|(1 << RXEN0);
	UCSR0C = (1 << UCSZ01)|(1 << UCSZ00);							//Asynchronous; 8 data bits, no parity
	UCSR1B = (1 << TXEN1)|(1 << RXEN1);
	UCSR1C = (1 << UCSZ11)|(1 << UCSZ10);
	UCSR2B = (1 << TXEN2)|(1 << RXEN2);
	UCSR2C = (1 << UCSZ21)|(1 << UCSZ20);
	//Set interrupt vectors enabled
	//UCSR0B |= (1 << RXCIE0);
	//UCSR1B |= (1 << RXCIE1);
	
	//Enable LEDs
	ddrDEBUGled = 0xFF;	//all outputs
	prtDEBUGled = 0x00;	//all low
	ddrDEBUGled2 |= (1 << bnDBG10)|(1 << bnDBG9)|(1 << bnDBG8);
	prtDEBUGled2 &= ~((1 << bnDBG10)|(1 << bnDBG9)|(1 << bnDBG8));
	
	//Enable GPIO lines used and not used to inputs with pull down (extra ones already enabled that way.
	ddrBBio1 &= ~(1 << bnG8BB17);		//This is an input
	ddrWAVRio |= (1 << bnG0W3);			//This pin alerts the WAVR that we are in fact running
	prtWAVRio |= (1 << bnG0W3);			//We are running, raise the IO line
	
	
	//Disable power to most peripherals that may be powered off.
	PRR0 |= (1 << PRTWI)|(1 << PRTIM1)|(1 << PRTIM0)|(1 << PRADC)|(1 << PRSPI);  //Turn EVERYTHING off initially except USART0(UART0)

	//Set up interrupts
	EICRA = (1 << ISC01)|(1 << ISC00)|(1 << ISC11)|(1 << ISC10);			//rising edge of INT0 and INT1 enables interrupt
	EICRB = (1 << ISC51)|(1 << ISC50)|(1  << ISC61)|(1 << ISC60)|(1 << ISC71)|(1 << ISC70);			//rising edge of INT7 and INT6 and INT5 enables interrupt
	__killCommINT();
	__killSpeedINT();
	EIMSK |= (1 << INT7);													//let it know if shutdown is imminent
	

	//Initialize flags
	justStarted=fTrue;
	flagQUIT=fFalse;
	flagGetUserClock=fFalse;
	flagWAVRtime=fFalse;
	flagGetWAVRtime=fFalse;
	flagInvalidDateTime=fFalse;
	flagUpdateWAVRtime=fFalse;
	
	flagReceiveWAVR=fFalse;
	flagReceiveBone=fFalse;
	flagReceiveBone2=fFalse;
	flagWaitingForWAVR=fFalse;
	flagWaitingForBone=fFalse;
	flagWaitingForBone2=fFalse;
	flagSendWAVR=fFalse;
	flagSendBone=fFalse;
	//flagNewTripStartup=fFalse;

	flagLCDinput=fTrue;
	flagUSBinserted=fFalse;
	flagUpdateWheelSize=fFalse;
	flagTripOver=fFalse;
	flagDeleteGAVRTrip=fFalse;
	flagDeleteUSBTrip=fFalse;

	flagHaveUSBTrips=fFalse;
	flagOffloadTrip=fFalse;
	flagNeedTrips=fFalse;
	flagViewUSBTrips=fFalse;
	flagViewGAVRtrips=fFalse;

	flagShowStats=fFalse;
	//Enable Communication interrupts now.
	__enableCommINT();

	
}
/*************************************************************************************************************/
void EnableRTCTimer(){
	//Asynchronous should be done based on TOSC1 and TOSC2
	//Give power back to Timer2
	PRR0 &= ~(1 << PRTIM2);
	Wait_ms(2);	//give it time to power on
	
	//Set to Asynchronous mode, uses TOSC1/TOSC2 pins
	ASSR |= (1 << AS2);
	
	//Set prescaler, initialize registers
	TCCR2B |= (1 << CS22)|(1 << CS20);	//128 prescaler, should click into overflow every second
	while ((ASSR & ((1 << TCR2BUB)|(1 << TCN2UB))));	//wait for it not to be busy
	TIFR2 = (1 << TOV2);								//Clear any interrupts pending for the timer
	TIMSK2 = (1 << TOIE2);								//Enable overflow on it
	
	//Away we go
}


/************************************************************************************************************/

void initSpeedSensing(){
	//Initialize Timer 1(16-bit), counter is read on an interrupt to measure speed. assumes rider is going  above a certain speed for initial test.
	PRR0 &= ~(1 << PRTIM1);
	TCCR1B |= (1 << CS12); 				//Prescaler of 256 for system clock
	TIFR1= (1 << TOV2);					//Make sure the overflow flag is not already set
	TCNT1 = 0x00;
	TIMSK1=(1 << TOIE2);

	__enableSpeedINT();
}
/*************************************************************************************************************/
void initHRSensing(){
	//Initialize timer 0, counter compare on TCNTA compare equals
	PRR0 &= ~(1 << PRTIM0);
	TCCR0A = (1 << WGM01);				//OCRA good, TOV set on top. TCNT2 cleared when match occurs
	TCCR0B = (1 << CS02);				//clk/256
	OCR0A = ONE_MS*NUM_MS;				//Number of Milliseconds
	TCNT0 = 0x00;						//Initialize
	TIMSK0 = (1 << OCIE0A);				//enable OCIE2A
}
/*************************************************************************************************************/
WORD GetADC(){
	volatile WORD ADCreading=0;

	
	//Take two ADC readings, throw the first one out.
	for (int i=0; i<2; i++){ADCSRA |= (1 << ADSC); while(ADCSRA & (1 << ADSC));}
	
	//Get the last ADC reading.	
	ADCreading = ADCL;
	ADCreading |= (ADCH << 8);
	
/*	if (reps++>500){
		flagUpdateUserStats=fTrue;
		reps=0;
	}*/
	/*****Debugging******
	volatile static WORD reps=0;
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
	return ADCreading;
}
	
/*************************************************************************************************************/
/*************************************************************************************************************/
