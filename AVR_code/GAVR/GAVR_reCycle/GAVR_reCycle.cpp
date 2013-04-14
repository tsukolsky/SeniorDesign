/*******************************************************************************\
| GAVR_reCycle.cpp
| Author: Todd Sukolsky
| Initial Build: 2/12/2013
| Last Revised: 4/14/13
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
\*******************************************************************************/

using namespace std;

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
#include "myUart.h"


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
#define __killCommINT() EIMSK &= ~((1 << INT0)|(1 << INT1))		//disable any interrupt that might screw up reception. Keep the clock ticks going though
#define __enableCommINT()  EIMSK |= (1 << INT0)|(1 << INT1)
#define __killSpeedINT() EIMSK &= ~(1 << INT6)
#define __enableSpeedINT() EIMSK |= (1 << INT6)

/*****************************************/
/**			Speed and HR Defs			**/
/*****************************************/
#define TIMER1_OFFSET		65535
#define BAD_SPEED_THRESH	4
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
BOOL flagReceiveWAVR, flagSendWAVR, flagReceiveBone, flagSendBone, flagWaitingForWAVR, flagWaitingForBone, flagNewTripStartup;
/* flagReceiveWAVR: We are receiving, or about to receive, transmission from WAVR								*/
/* flagSendWAVR: Sending to WAVR, or about to. We need to ask the WAVR for date/time or send it the date and	*/
/*				  time it asked for.																			*/
/* flagReceiveBone: Receiving, or about to receive, the transmission from the BeagleBone						*/
/* flagSendBone: Sending to, or about to send, information to the Bone											*/
/* flagWaitingForWAVR: Waiting to receive a UART string from WAVR, let main program know that with this flag.	*/
/* flagWaitingForBone: Waiting to receive a UART string from the BONE, this alerts main program.				*/
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
/* flagDeleteBoneTrip: Delete a trip on the BeagleBone.															*/
/*==============================================================================================================*/

/*==============================================================================================================*/
BOOL flagHaveUSBTrips, flagOffloadTrip, flagNeedTrips, flagViewBoneTrips, flagViewGAVRtrips;
/* flagHaveUSBTrips: Whether or not there are USB trips in the USBtripsViewer vector							*/
/* flagOffloadTrip:	We are sending one of our trips to the BeagleBone.											*/
/* flagNeedTrips: User wants to see the trips, ask for them.													*/
/*==============================================================================================================*/

/*****************************************/
/**			Global Variables			**/
/*****************************************/
myTime currentTime;			//The clock, must be global. Initiated as nothing.
trip globalTrip;			//Trip must be global as well, initialed in normal way. Startup procedure sets it. 
myVector<char *> USBtripsViewer;	//Vector to hold trip data sent over by the USB
myVector<char *> USBtripsHolder;

WORD numberOfSpeedOverflows=0;
BOOL flagNoSpeed=fFalse;
BOOL flagShowStats=fFalse;
WORD HRSAMPLES[300];

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
//INT6-> Speed magnet went around.
ISR(INT6_vect){
	//Do something with speed/odometer/trip thing.
	cli();
	WORD value=TCNT1;
	static int counter=0;
	if (value > 2){
		prtDEBUGled |= (1 << bnDBG1);
		if (flagNoSpeed){
			flagNoSpeed=fFalse;
			globalTrip.resetSpeedPoints();
		} else {
			globalTrip.addSpeedDataPoint(value+numberOfSpeedOverflows*TIMER1_OFFSET);
		}
		numberOfSpeedOverflows=0;	
		if (counter++ >= 9){flagShowStats=fTrue;counter=0;}
		Wait_ms(50);
		prtDEBUGled &= ~(1 << bnDBG1);
	}	
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
	//Declare variables
	volatile WORD signal=0;
	volatile static unsigned int location=0;
	
	//Increment N (time should reflect number of ms between timer interrupts), get ADC reading, see if newSample is good for anything.
	signal = GetADC();		//retrieves ADC reading on ADC0
	HRSAMPLES[location++]=signal;
	if (location >= 300){location=0; globalTrip.calculateHR(HRSAMPLES, 300);}

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
	volatile static int WAVRtimeout=0, BONEtimeout=0, startupTimeout=0, sendingWAVRtimeout=0;
	prtDEBUGled2 ^= (1 << bnDBG10);
	
	//volatile static int timeOut = 0;
	currentTime.addSeconds(1);
	//Timeout functionality
			
	//If WAVR receive timout is reached, either waiting or actually sending, reset the timeout, bring flags down and enable interrupt pins again		
	if (WAVRtimeout <= COMM_TIMEOUT_SEC && (flagReceiveWAVR || flagWaitingForWAVR)){WAVRtimeout++;}
	else if ((WAVRtimeout > COMM_TIMEOUT_SEC) && (flagReceiveWAVR || flagWaitingForWAVR)){WAVRtimeout=0; flagReceiveWAVR=fFalse; flagWaitingForWAVR=fFalse; __enableCommINT();}
	else if (!flagReceiveWAVR && !flagWaitingForWAVR && WAVRtimeout > 0){WAVRtimeout=0;}	//Both flags aren't set, make sure the timeout is 0
	else;
	
	if (BONEtimeout <= COMM_TIMEOUT_SEC && (flagReceiveBone || flagWaitingForBone)){BONEtimeout++;}	
	else if (BONEtimeout > COMM_TIMEOUT_SEC && (flagReceiveBone || flagWaitingForBone)){BONEtimeout=0; flagReceiveBone=fFalse; flagWaitingForBone=fFalse; __enableCommINT();}
	else if (!flagReceiveBone && !flagWaitingForBone && BONEtimeout >0){BONEtimeout=0;}
	else;
	
	//If sending to WAVR, institue the timeout
	if (flagSendWAVR && sendingWAVRtimeout <= COMM_TIMEOUT_SEC){sendingWAVRtimeout++;}
	else if (flagSendWAVR && sendingWAVRtimeout > COMM_TIMEOUT_SEC){sendingWAVRtimeout=0;flagSendWAVR=fFalse; __enableCommINT();}		//this doesn't allow for a resend.
	else if (!flagSendWAVR && sendingWAVRtimeout > 0){sendingWAVRtimeout=0;}
	else;
	
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
	if (flagNewTripStartup){PrintBone("Resuming an old trip");}
	else {PrintBone("Starting a new trip");}
	sei();
    while(fTrue)
    {	
		//Receiving from WAVR. Either a time string or asking user to set date/time/both.
		if (flagReceiveWAVR && !flagQUIT && !flagReceiveBone){
			prtDEBUGled2 |= (1 << bnDBG9);		//second from bottom, next to RTC...
			ReceiveWAVR();
			Wait_sec(2);
			if (!flagReceiveBone && !flagWaitingForBone){		//re enable comm interrupts only if we aren't waiting for one currently, or are about to go into one.
				__enableCommINT();
			}
			prtDEBUGled2 &= ~(1 << bnDBG9);
		}
		
		//Receiving from the Bone. Bone has priority over WAVR
		if (flagReceiveBone && !flagQUIT){
			prtDEBUGled2 |= (1 << bnDBG8);		//third from bottom.
			ReceiveBone();
			Wait_sec(2);
			if (!flagReceiveWAVR && !flagWaitingForWAVR){
				__enableCommINT();
			}
			prtDEBUGled2 &= ~(1 << bnDBG8);	
		}		
			
		
		//Send either "I need the date or time" or "Here is the date and time you asked for. If we are still waiting for WAVR, then don't send anything.
		if ((flagGetWAVRtime || flagUpdateWAVRtime) && !flagWaitingForWAVR && !flagQUIT){
			prtDEBUGled |= (1 << bnDBG2);		//third from top.
			__killCommINT();
			PrintBone("Sending to WAVR.");
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
		
		/* Debugging/PRE-LCD functionality */
		if (flagShowStats && !flagWaitingForBone && !flagWaitingForWAVR && !flagReceiveBone && !flagReceiveWAVR && !flagQUIT){
			cli();
			flagShowStats=0;
			double speed=globalTrip.getCurrentSpeed();
			double aveSpeed=globalTrip.getAverageSpeed();
			double distance=globalTrip.getDistance();
			double currentHR=globalTrip.getCurrentHR();
			double averageHR=globalTrip.getAveHR();
			char speedString[8],aveSpeedString[8],distanceString[8],currentHRstring[8],aveHRstring[8];
			PrintBone("***");
			PrintBone("Time/Date:");
			printTimeDate(fTrue,fTrue,fTrue);
			dtostrf(speed,5,3,speedString);
			dtostrf(aveSpeed,5,3,aveSpeedString);
			dtostrf(distance,7,4,distanceString);
			dtostrf(currentHR,4,2,currentHRstring);
			dtostrf(averageHR,4,2,aveHRstring);
			PrintBone("--Sp:");
			PrintBone(speedString);
			PrintBone("--AvSp:");
			PrintBone(aveSpeedString);
			PrintBone("--Di:");
			PrintBone(distanceString);
			PrintBone("--HR:");
			PrintBone(currentHRstring);
			PrintBone("--AvHR:");
			PrintBone(aveHRstring);
			PrintBone("*** ");
			sei();
		}
		//Here is where we react to what the LCD gets as inputs.		
		if (flagLCDinput){
			//User changed the wheel size
			if (flagUpdateWheelSize){
				double tempWheelSize;
				globalTrip.setWheelSize(tempWheelSize);
			}				
			if (flagTripOver){						//If the trip is over, end this one then start a new one.
				EndTripEEPROM();
				StartNewTripEEPROM();
			}				
			//If there is a USB and user asks to delete trips, offload a trip, or view trips on the USB, tell the Bone.
			if (flagUSBinserted && (flagDeleteUSBTrip || flagNeedTrips || flagOffloadTrip)){
				unsigned int whichGAVRTrip=1,whichUSBTrip=2;
				//We want to view the trips, get them and push them onto a template stack.? or vector push?->Only looking at start date in first implementation(use char vector).
				if (flagNeedTrips){
					SendBone(0);
				}
				//Get which trip the user wants to offload and send it to the Bone
				if (flagOffloadTrip && !flagDeleteUSBTrip){
					SendBone(whichGAVRTrip);
				//Tell the beaglebone to delete one of it's trips.
				} else if (flagDeleteUSBTrip && !flagOffloadTrip){
					//tell the bone to delete the trip.
					SendBone(whichUSBTrip);
					
					//Delete that trip from the USBtripsViewer
					BYTE numberOfUSBTrips=USBtripsViewer.size();
					
					//If there are 4 trips and we want to delete trip two, we pop trips 4->3 into new buffer, then pop oen to delete, then push 3->4 back on (now 2->3).
					BYTE numberOfPops=numberOfUSBTrips-whichUSBTrip;
					char * tempString;
					for (int i=0; i<numberOfPops;i++){
						tempString=USBtripsViewer.pop_back();
						USBtripsHolder.push_back(tempString);
					}
					//Pop the one to delete off
					tempString=USBtripsViewer.pop_back();
					
					//Push them back onto the USBtripsViewer.
					for (int i=0; i<numberOfPops; i++){
						tempString=USBtripsHolder.pop_back();
						USBtripsViewer.push_back(tempString);
					}
					
				} else;
				//If we need to delete a trip from the GAVR
				if (flagDeleteGAVRTrip){
					//delete the trip
					DeleteTrip(whichGAVRTrip);
					flagDeleteGAVRTrip=fFalse;
				}
				//If user wants to see the trips on the GAVR, show them.
				if (flagViewGAVRtrips){
				}
				//If user wants to see the trips on the Bone, show them if we have them
				if (flagViewBoneTrips && !flagNeedTrips && flagHaveUSBtrips){
				}
			}//end if USBinserted && flagOffloatTrip
			
		}	
		
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
	
	//Set BAUD rate of UART
	UBRR0L = ubrr;   												//set low byte of baud rate for uart0
	UBRR0H = (ubrr >> 8);											//set high byte of baud rate for uart0
	//UCSR0A |= (1 << U2X0);										//set high speed baud clock, in ASYNC mode
	UBRR1L = ubrr;													//set low byte of baud for uart1
	UBRR1H = (ubrr >> 8);											//set high byte of baud for uart1
	//UCSR1A |= (1 << U2X1);										//set high speed baud clock, in ASYNC mode			
	//Enable UART_TX0, UART_RX0, UART_RX1, UART_TX1
	UCSR0B = (1 << TXEN0)|(1 << RXEN0);
	UCSR0C = (1 << UCSZ01)|(1 << UCSZ00);							//Asynchronous; 8 data bits, no parity
	UCSR1B = (1 << TXEN1)|(1 << RXEN1);
	UCSR1C = (1 << UCSZ11)|(1 << UCSZ10);
	//Set interrupt vectors enabled
	//UCSR0B |= (1 << RXCIE0);
	//UCSR1B |= (1 << RXCIE1);
	
	//Enable LEDs
	ddrDEBUGled = 0xFF;	//all outputs
	prtDEBUGled = 0x00;	//all low
	ddrDEBUGled2 |= (1 << bnDBG10)|(1 << bnDBG9)|(1 << bnDBG8);
	prtDEBUGled2 &= ~((1 << bnDBG10)|(1 << bnDBG9)|(1 << bnDBG8));
	
	//Disable power to most peripherals that may be powered off.
	PRR0 |= (1 << PRTWI)|(1 << PRTIM1)|(1 << PRTIM0)|(1 << PRADC)|(1 << PRSPI);  //Turn EVERYTHING off initially except USART0(UART0)

	//Set up interrupts
	EICRA = (1 << ISC01)|(1 << ISC00)|(1 << ISC11)|(1 << ISC10);			//rising edge of INT0 and INT1 enables interrupt
	EICRB = (1  << ISC61)|(1 << ISC60)|(1 << ISC71)|(1 << ISC70);			//rising edge of INT7 and INT6 enables interrupt
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
	flagWaitingForWAVR=fFalse;
	flagWaitingForBone=fFalse;
	flagSendWAVR=fFalse;
	flagSendBone=fFalse;
	//flagNewTripStartup=fFalse;

	flagLCDinput=fFalse;
	flagUSBinserted=fFalse;
	flagUpdpateWheelSize=fFalse;
	flagTripOver=fFalse;
	flagDeleteGAVRtrip=fFalse;
	flagDeleteUSBTrip=fFalse;

	flagHaveUSBTrips=fFalse;
	flagOffloadTrip=fFalse;
	flagNeedTrips=fFalse;
	flagViewBoneTrips=fFalse;
	flagViewGAVRtrips=fFalse;

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
