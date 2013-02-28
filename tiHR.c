/** ****************************************************************************
 *	@file HRM_G2xx.c
 *
 * 	@mainpage Heart-Rate Monitor BoosterPack Software
 *
 * 	@detail	Description:
 *	Heart-Rate Monitor BoosterPack Software Implementation on the
 *  LaunchPad Development Platform with MSP430G2xx target platform.
 *  ACLK = N/A, MCLK = SMCLK = 500kHz, UART Baud Rate = 9600
 *
 *  @author A. Joshi / S. Ravindran / A. Miller
 *  @author Texas Instruments Inc.
 *  @date January 2011
 *	@version Version 1.0 - Initial Version
 *  @note Built with CCE Version: 4.1.2.0007 (with MSP430 Code Generation
 *  Tools v3.3.3) and IAR Embedded Workbench 5.10.1
 *  ***************************************************************************/

/*
 * THIS PROGRAM IS PROVIDED "AS IS". TI MAKES NO WARRANTIES OR
 * REPRESENTATIONS, EITHER EXPRESS, IMPLIED OR STATUTORY,
 * INCLUDING ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR
 * COMPLETENESS OF RESPONSES, RESULTS AND LACK OF NEGLIGENCE.
 * TI DISCLAIMS ANY WARRANTY OF TITLE, QUIET ENJOYMENT, QUIET
 * POSSESSION, AND NON-INFRINGEMENT OF ANY THIRD PARTY
 * INTELLECTUAL PROPERTY RIGHTS WITH REGARD TO THE PROGRAM OR
 * YOUR USE OF THE PROGRAM.
 *
 * IN NO EVENT SHALL TI BE LIABLE FOR ANY SPECIAL, INCIDENTAL,
 * CONSEQUENTIAL OR INDIRECT DAMAGES, HOWEVER CAUSED, ON ANY
 * THEORY OF LIABILITY AND WHETHER OR NOT TI HAS BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGES, ARISING IN ANY WAY OUT
 * OF THIS AGREEMENT, THE PROGRAM, OR YOUR USE OF THE PROGRAM.
 * EXCLUDED DAMAGES INCLUDE, BUT ARE NOT LIMITED TO, COST OF
 * REMOVAL OR REINSTALLATION, COMPUTER TIME, LABOR COSTS, LOSS
 * OF GOODWILL, LOSS OF PROFITS, LOSS OF SAVINGS, OR LOSS OF
 * USE OR INTERRUPTION OF BUSINESS. IN NO EVENT WILL TI'S
 * AGGREGATE LIABILITY UNDER THIS AGREEMENT OR ARISING OUT OF
 * YOUR USE OF THE PROGRAM EXCEED FIVE HUNDRED DOLLARS
 * (U.S.$500).
 *
 * Unless otherwise stated, the Program written and copyrighted
 * by Texas Instruments is distributed as "freeware".  You may,
 * only under TI's copyright in the Program, use and modify the
 * Program without any charge or restriction.  You may
 * distribute to third parties, provided that you transfer a
 * copy of this license to the third party and the third party
 * agrees to these terms by its first use of the Program. You
 * must reproduce the copyright notice and any other legend of
 * ownership on each copy or partial copy, of the Program.
 *
 * You acknowledge and agree that the Program contains
 * copyrighted material, trade secrets and other TI proprietary
 * information and is protected by copyright laws,
 * international copyright treaties, and trade secret laws, as
 * well as other intellectual property laws.  To protect TI's
 * rights in the Program, you agree not to decompile, reverse
 * engineer, disassemble or otherwise translate any object code
 * versions of the Program to a human-readable form.  You agree
 * that in no event will you alter, remove or destroy any
 * copyright notice included in the Program.  TI reserves all
 * rights not specifically granted under this license. Except
 * as specifically provided herein, nothing in this agreement
 * shall be construed as conferring by implication, estoppel,
 * or otherwise, upon you, any license or other right under any
 * TI patents, copyrights or trade secrets.
 *
 * You may not use the Program in non-TI devices.
 *
 * The ECCN is EAR99.
 *
 */

// ******** Definitions ******************************************************//

// Hand Detection Interval = 30 samples of EKG X 0.016 ms of sampling period
// X twice the length of target interval
#define HAND_DETECT_INTERVAL_SMALL			15
#define HAND_DETECT_INTERVAL_MEDIUM			24
#define HAND_DETECT_INTERVAL_LARGE			35

// 300ms Delay/Sampling Period
#define POST_HAND_DET_DELAY					300/16

// Max Timeout for no contact before the system restarts all over
#define EKG_MAX_NO_CONTACT					800/1

// 3000ms Delay/Sampling Period
#define INIT_HAND_DET_DELAY					3000/16.667

// 500kHz/52 = 9600 Baud
#define CYCLES_PER_SYMBOL 					52

#define RED_LED								BIT0
#define TX_PIN 								BIT1
#define RX_PIN								BIT2
#define SMCLK								BIT4
#define ADC_INCH_4							BIT4
#define ADC_INCH_5							BIT5
#define GREEN_LED							BIT6
#define VDD									BIT6
#define GROUND								BIT7
#define SHUTDOWN							BIT7
#define CHECK_HAND_DET						1
#define CONTACT								1
#define RESET								0
#define EKG_DATA_READY						1
#define EKG_DATA_RESET						0
#define POST_EKG_CHECK_HAND_DET				1
#define MAX_NUM_SAMPLES						30
#define NO_CONTACT                      	0
#define BAD_CONTACT                     	1
#define GOOD_CONTACT                    	2
#define SAMPLE_EKG                      	3

// High Threshold = 527 * 3.3 / 1023 = 1.7 V
#define HAND_DETECT_ADC_THRESHOLD_HIGH  	527

// Low Threshold = 450 * 3.3 / 1023 = 1.45 V
#define HAND_DETECT_ADC_THRESHOLD_LOW   	450

// Include Library Header Files
#include <msp430g2452.h>

// Function Prototypes *******************************************************//
void InitializeMSP430(void);
void EnableHandDetection (void);
void ClearHistoricalData (void);
void DisableHandDetection (void);
void PeakDetection (unsigned int *);
void PeakProcessing (void);
void TransmitHeartRateByUART (unsigned int Convert_BPM);
void TxByteByUART (unsigned char BPM);
void SetADCToSampleEKG(void);
void SetADCToSampleHandDetect(void);
void SetWDTSourceACLKFromVLO(void);
void SetWDTSourceMCLKFromDCO (void);
void HeartRateMonitorCore(void);

// Global Variables **********************************************************//

// cnt=1 => first time HR proc; ncnt keeps track of when to TX HR
unsigned char ncnt=1, fac=25, L, P;

// 70 Beats per Minute: Avg. person
unsigned char HR[3] = {70, 70, 70};

// 'pks' store peaks per block and 'df' difference between peaks
unsigned char pks[2], df[8];

/**
 * @brief This variable to used to keep track of the minimum EKG peak distance
 * to minimize the possibility of false peaks being considered in the BPM
 * calculation.
 * Lower min_pks_dst (18 for 60 Hz) allows detection of upto 180 BPM
 * Higher min_pks_dst gives more robustness to the algorithm
 *
 * Default value for Min peaks distance = 18
 */
unsigned char min_pks_dst=18;

/**
 * @brief This variable to used to represent the number of EKG peaks used in
 * BPM post-processing calculation.
 * Larger block_length gives more robustness
 * Smaller block_length improves algorithm response time
 */
unsigned int block_length=6;

/**
 * @brief This variable represents the maximum permitted variation between
 * successive BPM outputs
 * Larger b_max allows faster convergence to target value
 * Smaller b_max improves algorithm robustness and smoother output display
 */
unsigned char b_max=3;

/**
 * @brief This variable controls the timeout before the b_max limit on the BPM
 * variation kicks in.
 * Smaller value lets b_max kick in faster and leads to a more controlled,
 * smoother output (more algorithm robustness)
 * Larger value allows more time for ramp up or ramp down (quicker settling
 * time)
 */
unsigned char timer1 = 19;                  // approximately 4 seconds
unsigned char timer2 = 17;                  // 8.5 seconds

/**
 * @brief This variable represents the amplitude threshold factor.
 * Smaller value able to accommodate smaller QRS peaks (but higher false positives)
 * Larger value keeps out more of the spurious peaks (but higher false negatives)
 */
unsigned short v_thres=0;

// 30 samples in data array
unsigned int d[30];

unsigned char post_EKG_hand_det_flag = 0;
unsigned short denom=70;
unsigned char hand_det_flag = 0;
unsigned char initial_contact = 0;
unsigned char quality_of_contact_flag = NO_CONTACT;
unsigned char numer;
unsigned char Hout[2] = {70,70}, H1out = 70;
unsigned char tmp=70, prev_tmp=70;
signed char dcnt=-1;
unsigned char sample_index=0;
unsigned char block_ready=0;
unsigned int sample;
unsigned int no_contact_count = 0;
unsigned int post_hand_det_vcnt = 0;
unsigned int sample_contact;
unsigned int init_hand_det_del = 0;
unsigned long V_array[9], vals[2];
unsigned int hand_flag=0;
unsigned int hand_detection_interval=HAND_DETECT_INTERVAL_SMALL;
unsigned int Convert_BPM;
unsigned int L_cnt=0;

/**
 * @brief Function Name: main
 * @brief Description: The main() function of the program.
 * Initializes the peripheral blocks and then places the MSP430 in low power
 * mode. The Watchdog Timer (set in interval timer mode) wakes the MSP430 up
 * periodically, checks for hand-detection, and switches to EKG sampling mode
 * or goes back to low power (sleep) mode.
 *
 * @param Input Parameters: None
 * @return Return Values: None
 **/
void main(void)
{
	// Initialize the MSP430 peripherals
    InitializeMSP430();
	SetADCToSampleHandDetect();
	SetWDTSourceACLKFromVLO();

	// Place the MSP430 in LPM3 with interrupts enabled
  	__bis_SR_register(LPM3_bits + GIE);

	// Run Forever
	for(;;)
	{
		// Run the Heart Rate Monitor Core Algorithm
		HeartRateMonitorCore();
	}

}

/**
 * @brief Function Name: InitializeMSP430
 * @brief Description: Initializes all of the peripheral blocks within the
 * MSP430. Initialize all GPIO port bits as outputs to low state. Initialize the
 * UART pins for RX/TX operation. Loads the DCO with internal calibrated setting
 * for 1 MHz operation. Initializes the system clock (SMCLK & MCLK) to 500 kHz.
 * Initialize the ADC and select the reference, conversion mode and
 * sample-hold source. Initializes Timer A in compare mode with CCI0A as the
 * input signal, output mode is determined by the OUT bit, the OUT bit is
 * set to output low, and the capture/compare interrupt is disabled.
 * The Timer A control register is setup to source SMCLK clock and to count
 * in continuous mode. Initialize the LED ports as outputs in low state.
 * @param Input Parameters: None
 * @return Return Values: None
 **/
void InitializeMSP430 (void)
{
    WDTCTL = WDTPW + WDTHOLD;		// Halt the Watchdog Timer

	// Initialize all GPIO port bits as outputs to low state.
	// Port 1
	P1REN = 0;				// Pull-Up/Pull-down Resistors Disabled on P1 ports
	P1SEL = 0;				// Clear any previous settings
	P1DIR = 0;				// Clear any previous settings
	P1OUT = 0;				// Initialize port outputs to low state

	// Port 2
	P2REN = 0;				// Pull-Up/Pull-down Resistors Disabled on P2 ports
	P2SEL = 0;				// Clear any previous settings
	P2DIR = 0;				// Clear any previous settings
	P2OUT = 0;				// Initialize port outputs to low state

	// Initialize the UART pins for RX/TX operation.
	P1DIR |= TX_PIN;		// Initialize the TX pin as output for UART
	P1OUT &= ~(TX_PIN); 	// Set the TX pin output to low
	P1DIR &= ~(RX_PIN);		// Initialize the RX pin as input for UART

	P2DIR |= SHUTDOWN;		// Configure the Shutdown pin as output
	P2OUT &= ~(SHUTDOWN);	// Set the Shutdown pin to low

	// Loads the DCO with internal calibrated setting  for 1 MHz operation.
	// Initializes the system clock (SMCLK & MCLK) to 500 kHz.
    DCOCTL  = CALDCO_1MHZ;			// Use internally calibrated DCO settings
	BCSCTL1 = CALBC1_1MHZ;
	BCSCTL2 |= DIVS_1;				// Divide factor of 2 => 1MHz/2 = 500kHz

	// Initialize the ADC and select the reference, conversion mode and
	// sample-hold source.
	ADC10CTL0 |= ADC10SHT_3 			// Sample hold time = 64 ADC10CLKs,
			   + SREF_0;				// Select reference VR+ = Vcc VR- = Vss

	ADC10CTL1 |= SHS_0 					// ADC10SC bit being toggled by WDT
			   + CONSEQ_0;        		// Single Channel Single Conversion

	/* Initializes Timer A in compare mode with CCI0A as the
	 * input signal, output mode is determined by the OUT bit, the OUT bit is
	 * set to output low, and the capture/compare interrupt is disabled.
	 * The Timer A control register is setup to source SMCLK clock and to count
	 * in continuous mode. */
	// Select timer source as SMCLK
	TACTL = TASSEL_2;
	// Select timer to output mode 0 with OUT bit status
	CCTL0 = OUTMOD_0;

	// Initialize the LED ports as outputs in low state.
	P1DIR |= RED_LED + GREEN_LED;		// Setup LED ports as outputs
	P1OUT &= ~(RED_LED + GREEN_LED);	// Turn off LEDs
}

/**
 * @brief Function Name: HeartRateMonitorCore
 * @brief Description: This is the core algorithm that takes arrays of EKG
 * samples and computes the Beats Per Minute. The Watchdog Timer (WDT) is
 * sourced from ACLK/VLO, waking up the MSP430 periodically, and checking for
 * hand-detection. If there's no hand detection, the MSP430 is placed back in
 * the lowest power mode. If there's hand detected, the WDT is sourced from
 * MCLK/DCO, the ADC is then switched to sample the output of the Analog Front
 * End and triggered by the WDT, EKG samples are collected, followed by
 * Heart Beat Detection and processing algorithms. The final Beats Per Minute
 * value is output via Timer-A based UART.
 *
 * @param Input Parameters: None
 * @return Return Values: None
 **/
void HeartRateMonitorCore(void)
{
	unsigned char flg;
    unsigned char i;
    unsigned int dtmp[5];

 	// This will be 1 when ADC10 is done with conversion of sampled input
  	if (block_ready == EKG_DATA_READY)
  	{
      /* Store last 5 values in tmp variables (for overlap)*/
	  dtmp[0] = d[25];
	  dtmp[1] = d[26];
      dtmp[2] = d[27];
      dtmp[3] = d[28];
      dtmp[4] = d[29];

	  // Call PeakDetection
	  // Starting with the first element in d
	  PeakDetection(&d[0]);

      // This is the first entry, initialize P, V_array, df
	  if (dcnt==-1)
	  {
	  	// block_count==1
	    if (L > 1)						// when there is more than one peak
		{
			P = pks[1];
			df[0] = pks[1] - pks[0];
			V_array[0] = vals[0];
			V_array[1] = vals[1];
			dcnt=1;
		}
		else   // one peak
		{
			P = pks[0];
			V_array[0] = vals[0];
			dcnt=0;
		}
	  }

      else if (hand_flag==0 && L>0)  // block_count>1, after first entry
	  {
	  	L_cnt=0;
		flg=1; // Flag to determine if new peak addition happened

		// Check to see if minimum distance criterion is met
		// At least 300ms between peaks
		if ((pks[0]+fac) - P >= min_pks_dst)
		{
			// Store the difference in peak location
			df[dcnt] = pks[0] + fac - P;
			P = pks[0];
			dcnt++;
			V_array[dcnt] = vals[0];

			if (L>1)
			{
				df[dcnt] = pks[1]-pks[0];
				P = pks[1];
				dcnt++;
				V_array[dcnt] = vals[1];
				// 2 new peaks added
				flg++;
			}
			fac = 25; 							// hop length
		  }
		  else if (vals[0] >= V_array[dcnt])    // Check
		  {
		    if (dcnt>0)
			{
				df[dcnt-1] = df[dcnt-1] + (pks[0]+fac-P);
			}

			P = pks[0];
			V_array[dcnt] = vals[0];
			flg=0; // peak replaced, no addition of peak so set flag to 0
			fac = 25; // hop length
			if (L>1)
			{
				df[dcnt] = pks[1] - pks[0];
				P = pks[1];
				dcnt++;
				V_array[dcnt] = vals[1];
				flg=1; // peak added
			}
		  }
		  else if (L>1)
		  {
			// 2 peaks in frame but first one too close, but add the second
			df[dcnt] = pks[1]+fac-P;
			P = pks[1];
			dcnt++;
			V_array[dcnt] = vals[1];
			fac = 25; // hop length
		  }
		else
		{
			// no valid peaks
			flg=0;
			fac = fac+25;
		}
	  }
	  else if (hand_flag==1 && L>0)
	  {
	  	L_cnt=0;
	  	if (L>1)
	  	{
	  		df[dcnt]=pks[1]-pks[0];
	  		dcnt++;
	  		V_array[dcnt] = vals[1];
	  		flg=1;
	  		P = pks[1];
	  	}
	  	else
	  	{
	  		P = pks[0];
	  		flg=0;
	  	}
	  	hand_flag=0;
	  	fac=25;
	  }
	  else if (hand_flag==2 && L>0)
	  {
	  	flg=0;
	  	L_cnt=0;

	  	if (L>1)
	  	{
	  		P=pks[1];
	  		hand_flag=0;
	  		fac=25;
	  	}
	  	else
	  	{
	  		//L_cnt++;
	  		hand_flag=1;
	  	}
	  }
	  else // L=0 no valid peaks
	  {
	  	L_cnt++;
	  	flg=0;
	  	if (hand_flag==0)
	  	{fac=fac+25;}
	  	else if (hand_flag>0)
	  	{hand_flag--;}
	  }

	  // Block Count >= 7; ... at least seven possible peaks
	  if (dcnt>block_length)
	  {
        // post process the peaks
		PeakProcessing();

        if (numer>0)
          // raw heart rate estimate in BPM
          tmp = (unsigned short) (numer*3600/denom);
        else
          tmp = HR[2];

        if (!timer1)
        {
        	b_max = 3;
            if (prev_tmp-tmp > b_max)
              tmp = prev_tmp-b_max;
            else if (prev_tmp-tmp < b_max)
              tmp = prev_tmp+b_max;
            prev_tmp=tmp;
        }

        // weighted heart rate estimate
        Hout[1] = (12*tmp + 9*HR[2] + 7*HR[1] + 4*HR[0])>>5;

		// discard oldest HR and replace with newest estimate
		HR[0] = HR[1];
		HR[1] = HR[2];
        HR[2] = Hout[1];

		if (flg>0)
		{
			for(i=0;i<dcnt-flg; i++)
			{
				df[i] = df[i+flg];
				V_array[i] = V_array[i+flg];
			}
			dcnt=dcnt-flg;
			V_array[dcnt] = V_array[dcnt+flg];
		}
	  }
	  else
	  {
	  	// stick with previous raw estimate
        Hout[1] = (12*Hout[0] + 9*HR[2] + 7*HR[1] + 4*HR[0]) >>5;
		HR[0] = HR[1];
		HR[1] = HR[2];
        HR[2] = Hout[1];
	  }

      // Output HR every 1s
      if (ncnt > 1)
      {
			Convert_BPM = ((Hout[0]+Hout[1])>>1);
            TransmitHeartRateByUART(Convert_BPM);
            ncnt=0;
      }
      ncnt++;

      // Hout[0] is always the previous value of Hout[1]
      Hout[0] = Hout[1];

      if (timer1==0)
      {
      	/*
      	 * If the heart-beat rate is low, increase the hand-detection period
      	 * so that more EKG peaks can be sampled to compute accurate BPM value.
      	 * Otherwise, set it to a smaller interval for frequent periodic checks.
      	 */
      	if (Convert_BPM < 36)
      	{
      		hand_detection_interval = HAND_DETECT_INTERVAL_LARGE;
      	}
      	else if (Convert_BPM > 35 && Convert_BPM < 46)
      	{
      		hand_detection_interval = HAND_DETECT_INTERVAL_MEDIUM;
      	}
      	else
      	{
      		hand_detection_interval = HAND_DETECT_INTERVAL_SMALL;
      	}
      }


      if(timer1) {
 	  timer1--;
      }

	  if (timer2)
		timer2--;

			// Align 5 previous samples in new block
			d[0] = dtmp[0];
			d[1] = dtmp[1];
			d[2] = dtmp[2];
			d[3] = dtmp[3];
			d[4] = dtmp[4];

            sample_index = 5;
            block_ready = EKG_DATA_RESET;
			TACTL &= ~(MC_3);				// Disable Timer A

            // If the hand-detection interval has expired, set the ADC
			// to sample the hand-detection channel.
            if ( sample_contact == 0 )
            {
				SetADCToSampleHandDetect();
				initial_contact = RESET;
				post_hand_det_vcnt = RESET;
            	post_EKG_hand_det_flag = POST_EKG_CHECK_HAND_DET;
            }

            // Else continue to sample EKG
            SetWDTSourceMCLKFromDCO();
            __bis_SR_register(LPM0_bits);
            if (L_cnt==hand_detection_interval)
            {
            	//do reset
            	ClearHistoricalData();
            }
        }
}

/**
 * @brief Function Name: WATCHDOG_TIMER_ISR
 * @brief Description: ISR to service the interrupt when the watchdog timer
 * (set in interval counter mode) expires. The Watchdog Timer is used to trigger
 * the ADC in this program (as Timer A is used for UART communication). If the
 * program is in hand-detection mode, the LEDs are flashed briefly to indicate
 * contact status: Red - Bad contact | Green - Good contact | No LEDs - No
 * contact detected.
 * @param Input Parameters: None
 * @return Return Values: None
 **/
#pragma vector=WDT_VECTOR
__interrupt void WATCHDOG_TIMER_ISR (void)
{
	// Enable ADC for conversion
	ADC10CTL0 &= ~(ENC);		// Clear to change ADC settings
	ADC10CTL0 |= ADC10ON ;		// Turn on ADC10
	ADC10CTL0 |= ENC;			// Enable ADC conversions

	if (( initial_contact == RESET ))
	{
		EnableHandDetection();
	}

	if (( quality_of_contact_flag == GOOD_CONTACT )
	|| ( quality_of_contact_flag == BAD_CONTACT ))
	{
		SetWDTSourceMCLKFromDCO();
		SetADCToSampleEKG();
		quality_of_contact_flag = SAMPLE_EKG;
	}
	if (( post_EKG_hand_det_flag == POST_EKG_CHECK_HAND_DET )
	&& ( initial_contact == CONTACT ))
	{
		post_EKG_hand_det_flag = RESET;
		SetADCToSampleEKG();
		DisableHandDetection();
		quality_of_contact_flag = SAMPLE_EKG;
		if (Convert_BPM < 36)
		{
			hand_flag=0;
			dcnt=-1;
			fac=25;
			hand_detection_interval=HAND_DETECT_INTERVAL_LARGE;
		}
		else
		{
			hand_flag=2;
		}
		sample_index=0;
	}
	if ( quality_of_contact_flag == SAMPLE_EKG )
	{
		hand_det_flag = SAMPLE_EKG;
		++post_hand_det_vcnt;
	}

	// Turn the LEDs three-quarters of the way through the post hand-detection
	// delay. This will ensure no interference with the EKG output waveform.
	if( post_hand_det_vcnt > POST_HAND_DET_DELAY )
	{
		P1OUT &= ~(RED_LED + GREEN_LED);			// Green & RED LEDs off
	}

	if (( initial_contact == CONTACT )
	&& ( init_hand_det_del < INIT_HAND_DET_DELAY ))
	{
		++init_hand_det_del;
	}

	ADC10CTL0 |= ADC10SC + ADC10IE;					// 60Hz sampling
}


/**
 * @brief Function Name: ADC10_ISR
 * @brief Description: ISR to service the interrupt once ADC conversion has
 * been completed. If the program is in hand-detection mode, the result
 * is compared against the set thresholds to determine good/bad/no contact.
 * If the program is in EKG sampling mode, the result is pushed into the EKG
 * samples array.
 * @param Input Parameters: None
 * @return Return Values: None
 **/
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR (void)
{
	// Until contact is made keep doing this check
	if ( quality_of_contact_flag != SAMPLE_EKG )
	{
		// If the ADC Input Voltage during hand-detect is between 1.8V-3.3V,
        // then indicate good contact
		if ( ADC10MEM > HAND_DETECT_ADC_THRESHOLD_HIGH )
		{
			DisableHandDetection();
			P2OUT |= SHUTDOWN;							// Enable AFE
			hand_det_flag = RESET;
			no_contact_count = RESET;
			initial_contact = CONTACT;
			quality_of_contact_flag = GOOD_CONTACT;		// Good Contact
			P1OUT &= ~(RED_LED);						// Red LED off
			P1OUT |= GREEN_LED;							// Green LED on
			// Place the MSP430 in LPM0 mode
			__bic_SR_register_on_exit(LPM3_bits - LPM0_bits);

		}
		// If the ADC Input Voltage during hand-detect is between 1.5V-1.79V
        // then indicate bad contact
		else if ( ( ADC10MEM > HAND_DETECT_ADC_THRESHOLD_LOW )
		&& ( ADC10MEM <= HAND_DETECT_ADC_THRESHOLD_HIGH ) )
		{
			DisableHandDetection();
			P2OUT |= SHUTDOWN;							// Enable AFE
			hand_det_flag = RESET;
			no_contact_count = RESET;
			initial_contact = CONTACT;
			quality_of_contact_flag = BAD_CONTACT;		// Bad contact
			P1OUT &= ~(GREEN_LED);						// Green LED off
			P1OUT |= RED_LED;							// Red LED on
			// Place the MSP430 in LPM0 mode
			__bic_SR_register_on_exit(LPM3_bits - LPM0_bits);
		}
		// If the ADC Input Voltage during hand-detect is between 0V-1.49V
        // There is no contact
		else
		{
			quality_of_contact_flag = NO_CONTACT;		// No hands
			P1OUT &= ~(RED_LED + GREEN_LED);			// Green & RED LEDs off

			// Increment No Contact Counts only when sampling EKG
			if( hand_det_flag == SAMPLE_EKG )
			{
				++no_contact_count;
			}
            // If no contact has been detected for a certain timeout period,
            // reset everything and start again fresh from the beginning when
            // contact is detected again.
			if ( ( no_contact_count >= EKG_MAX_NO_CONTACT )
			&& ( hand_det_flag == SAMPLE_EKG ))
			{
				P2OUT &= ~(SHUTDOWN);		// Disable AFE
				ADC10CTL0 &= ~(ENC);		// Clear to change ADC settings
				ADC10CTL0 &= ~(ADC10ON) ;	// Disable the ADC to conserve power.
				SetWDTSourceACLKFromVLO();
				EnableHandDetection();
				SetADCToSampleHandDetect();
				no_contact_count = RESET;
				initial_contact = NO_CONTACT;
				init_hand_det_del = RESET;
				post_hand_det_vcnt = RESET;
            	ClearHistoricalData();
            	__bis_SR_register_on_exit(LPM3_bits);
			}
		}
	}

    // If in EKG Sampling mode, stuff the array with the ADC digital result
	else if (( quality_of_contact_flag == SAMPLE_EKG )
			&& ( init_hand_det_del >= INIT_HAND_DET_DELAY ))
	{
		sample = (unsigned int) ADC10MEM;			// Begin processing EKG
		// Truncate conversion result (10-bit) to a byte
		d[sample_index++] = sample;
		// When the 30 samples are stored
		if(sample_index == MAX_NUM_SAMPLES)
		{
			// Wake up on exit to process block
			block_ready = EKG_DATA_READY;
			--sample_contact;
			WDTCTL = WDTPW + WDTHOLD;		// Halt the Watchdog Timer
			TACTL |= MC_2;					// Select Continuous mode
			// Exit LPM0
			__bic_SR_register_on_exit(LPM0_bits);
		}
	}
}

/**
 * @brief Function Name: SetADCToSampleEKG
 * @brief Description: Set the ADC Channel to sample the output of the EKG
 * Analog Front End.
 * @param Input Parameters: None
 * @return Return Values: None
 **/
void SetADCToSampleEKG(void)
{
	ADC10CTL0 &= ~(ENC);		// Clear enable conversion bit to change settings
	ADC10CTL1 &= ~(INCH_5);		// Hand Detection Channel off
	ADC10CTL1 |= INCH_4;		// EKG Channel on
	ADC10AE0  &= ~(ADC_INCH_5);
	ADC10AE0  |= ADC_INCH_4;
	ADC10CTL0 |= ENC;			// Lock input channel settings
}

/**
 * @brief Function Name: SetADCToSampleHandDetect
 * @brief Description: Set the ADC Channel to sample the output of the Hand
 * Detection Circuit.
 * @param Input Parameters: None
 * @return Return Values: None
 **/
void SetADCToSampleHandDetect(void)
{
	ADC10CTL0 &= ~(ENC);		// Clear bit to change input channel settings
	ADC10CTL1 &= ~(INCH_4);		// EKG channel off
	ADC10AE0  &= ~(ADC_INCH_4);
	ADC10CTL1 |= INCH_5;		// Hand Detection channel on
	ADC10AE0  |= ADC_INCH_5;
	ADC10CTL0 |= ENC;			// Lock input channel settings
}

/**
 * @brief Function Name: SetWDTSourceACLKFromVLO
 * @brief Description: Select the WatchDog Timer to run from ACLK sourced from
 * the internal VLO oscillator.
 * @param Input Parameters: None
 * @return Return Values: None
 **/
void SetWDTSourceACLKFromVLO(void)
{
	hand_det_flag = CHECK_HAND_DET;
	BCSCTL3 |= LFXT1S_2;		// Select VLO
	WDTCTL = WDT_ADLY_250;		// Sample ADC10 @ 1.46Hz 12kHz/8192 = 1.46Hz
	IE1 |= WDTIE;
}

/**
 * @brief Function Name: SetWDTSourceMCLKFromDCO
 * @brief Description: Select the WatchDog Timer to run from MCLK sourced from
 * the internal DCO.
 * @param Input Parameters: None
 * @return Return Values: None
 **/
void SetWDTSourceMCLKFromDCO (void)
{
	WDTCTL = WDT_MDLY_8;		// Sample ADC10 @ 60Hz, 500kHz/8192 = 60Hz
	IE1 |= WDTIE;				// Enable WDT ISR
}

/**
 * @brief Function Name: ClearHistoricalData
 * @brief Description: Resets all variables back to their default values.
 * @param Input Parameters: None
 * @return Return Values: None
 **/
void ClearHistoricalData(void)
{
	unsigned char x;
    HR[0] = 70;
    HR[1] = 70;
    HR[2] = 70;
    denom=70;
    v_thres=0;
    Hout[0] = 70;
    Hout[1] = 70;
    H1out = 70;
    tmp=70, prev_tmp=70;
	sample_index=0;
	L=0;
	P=0;
	ncnt=1;
	fac=25;
	dcnt=-1;
	pks[0] = 0;
	pks[1] = 0;
	vals[0] = 0;
	vals[1] = 0;
	for(x=0; x<8;x++)
	{
          df[x] =0;
          V_array[x] = 0;
	}
	V_array[8]=0;
	timer1=19;
	timer2=17;
	hand_detection_interval=HAND_DETECT_INTERVAL_SMALL;
	block_length = 6;
}

/**
 * @brief Function Name: EnableHandDetection
 * @brief Description: Enables the power and ground ports for hand-detection
 * circuit.
 * @param Input Parameters: None
 * @return Return Values: None
 **/
void EnableHandDetection (void)
{
	quality_of_contact_flag = NO_CONTACT;
	sample_contact = hand_detection_interval;

	// Enable virtual ground port for hand-detection circuit
	P1DIR |= GROUND;
	P1OUT &= ~(GROUND);

	// Enable virtual power port for hand-detection circuit
	P2DIR |= VDD;
	P2OUT |= VDD;
}

/**
 * @brief Function Name: DisableHandDetection
 * @brief Description: Disables the power and ground ports for hand-detection
 * circuit.
 * @param Input Parameters: None
 * @return Return Values: None
 **/
void DisableHandDetection (void)
{
	P1DIR &= ~(GROUND);
	P2DIR &= ~(VDD);
	P2OUT &= ~(VDD);
}

/**
 * @brief Function Name: PeakDetection
 * @brief Description: This function detects the peaks of the EKG waveform and
 * checks for the validity of each peak that is detected.
 * @param Input Parameters: The address of the first element in the data array
 * d, (&d[0]).
 * @return Return Values: None
 **/
void PeakDetection(unsigned int *d)
{
	/* d is data array
	pks, vals are array of 2 to store location and value of peaks
	L is the number of peaks L=1,2 */

	unsigned short i;								// Used for loops
    unsigned long temp;
    unsigned char dst = min_pks_dst-1;  			// 18-1

	L = 1; 				 							// Peaks = 1

	//  pack difference and absolute value in same loop
	//  This avoids negative values for unsigned short
      for (i=0;i<29;i++)
	{
		if (*(d+i+1) > *(d+i))     				// For positive values of d
		{
			// *(d+i) becomes the difference for a positive result
			*(d+i) = *(d+i+1) - *(d+i);
		}
		else									// For negative values of d
		{
			// *(d+i) becomes the difference for a positive result
			*(d+i) = *(d+i) - *(d+i+1);
		}
	}

	// Do peak enhancement and max
	vals[0]=0;
    pks[0]=0;
    for (i=0;i<25;i++)
    {
	// Folded corr; N=5
	temp = (unsigned long)(((*(d+i))*(*(d+i+4))) + ((*(d+i+1))*(*(d+i+3))));

        // Find max, if temp is positive
        if (temp > vals[0])
		{
			// First element of vals set to temp for positive temp
			vals[0] = temp;

			// First element of pks is set to value of i for positive temp
			pks[0] = i;
		}
	}

	// Check for second peak
	if (pks[0] > dst)     						// When pks is greater than 17
	{
		// Make way for peak 1 (clear array location 0)
		vals[1] = vals[0];
		pks[1] = pks[0];
		vals[0] = (*(d))*(*(d+2));
		pks[0] = 0;
		for(i=1; i<(pks[1]-dst); i++)
		{
			// Folded corr; N=5
			temp = (unsigned long)(((*(d+i))*(*(d+i+4))) + ((*(d+i+1))*(*(d+i+3))));
                        if (temp > vals[0])
			{
				vals[0] = temp;
				pks[0] = i;
			}
		}
		// Check if peak is valid
		if (vals[0] > 3*(vals[1])/10 )
		{
			L = 2;
		}
		else
		{
			// Shift previous peak back to array location 0
			pks[0] = pks[1];
			vals[0] = vals[1];
		}
	}
	else if ( pks[0] < (25-dst-1))
	{
		i = pks[0]+dst+1;

		// Folded corr; N=5
		vals[1] = (unsigned long)(((*(d+i))*(*(d+i+4))) + ((*(d+i+1))*(*(d+i+3))));
		pks[1] = i;
		for(i=(pks[0]+dst+2);i<25;i++)
		{
			// Folded corr; N=5
			temp = (unsigned long)(((*(d+i))*(*(d+i+4))) + ((*(d+i+1))*(*(d+i+3))));
                        if (temp > vals[1])
			{
				vals[1] = temp;
				pks[1] = i;
			}
		}
		// Check if peak is valid
		if (vals[1] > 3*(vals[0])/10)
		{
			L = 2; // two peaks
		}
	}

	// additional amplitude check
	temp=vals[0];
	if (vals[1]>vals[0])
	{temp=vals[1];}

	if (temp < v_thres/5)
	{
		L=0;
	}


}

/**
 * @brief Function Name: PeakProcessing
 * @brief Description: This function calculates the average of the sampled peaks
 * used for heartbeat rate calculation.
 * @param Input Parameters: None
 * @return Return Values: None
 **/
void PeakProcessing(void)
{
	unsigned char i, factor;
	unsigned char x[9];
    unsigned long mean_sum;

	// If the measured heart-rate is less than 30 BPM
	if (H1out < 30)
      factor = 40;          // 40% of mean
    else
      factor = 35;			// 35% of mean

    // Find mean_sum
    mean_sum=0;
    for(i=0;i<dcnt+1;i++)
	{
		mean_sum += V_array[i];
	}

    // Compute 0.35 mean, or 0.40 mean if heart-rate is below 30 BPM
    v_thres = (unsigned short) ((factor*(mean_sum))/((dcnt+1)*100));

	// Find peaks that are too small
	numer=0; // Use numer as the counter
	for(i=0;i<dcnt+1;i++)
	{
		// For peak magnitudes that are greater than or equal to
		// 0.35 or 0.40 of the avg magnitude of all measured peaks
		if (V_array[i]>= v_thres)
		{
			x[numer] = i;  		// Valid peak
			(numer)++;
		}
	}
	(numer)--;

	// Find duration in samples between first and last peak
	denom=0;
	if (numer==0)
	{
		denom=1;
	}
	else
	{
		for (i=x[0];i<x[numer];i++)
		{
			denom = denom + df[i];
		}
	}


}

/**
 * @brief Function Name: TransmitHeartRateByUART
 * @brief Description: This function takes numbers between 0 and 199 and
 * converts the input number into individual ASCII digits. It is followed by a
 * space and the letters ‘BPM.’ If the number is less than 100, the first digit
 * is a space. This function passes each ASCII digit as a byte to the
 * TxByteByUART() function.
 * @param Input Parameters: Number of Beats Per Minute (Convert_BPM)
 * @return Return Values: None
 **/
void TransmitHeartRateByUART (unsigned int Convert_BPM)
{
	unsigned char BPM = 0;					// BPM int to char
	unsigned char output_byte[9];			// Array to parse BPM
	unsigned char buffer = 0;				// Buffer for manipulation of BPM
	unsigned char Output_index = 9;			// Array Index

	output_byte[0] = 10;					// Newline
	output_byte[1] = 13;					// Carriage Return
	output_byte[6] = 66;					// B
	output_byte[7] = 80;					// P
	output_byte[8] = 77;					// M

	if ( Output_index == 9 )
	{
		Output_index = 0;
 		if ( Convert_BPM < 100 )				// For BPM less than 100
 		{
 			output_byte[2] = 32;				// Space
			output_byte[3] = Convert_BPM/10;
			output_byte[4] = ( Convert_BPM - ( output_byte[3]*10 ) ) + 48;
			output_byte[3] = output_byte[3] + 48;
			output_byte[5] = 32;				// Space
		}
		else if ( Convert_BPM >= 100 )			// For BPM over 100
		{
			 output_byte[2] = 1 + 48;
			 buffer = Convert_BPM - 100;
			 output_byte[3] = buffer/10;
			 output_byte[4] = ( buffer - ( output_byte[3]*10 ) ) + 48;
			 output_byte[3] = output_byte[3] + 48;
			 output_byte[5] = 32;				// Space
		}
	}

	// Send each ASCII byte of data to UART for TX
	while ( Output_index < 9 )
	{
		BPM = (unsigned char) (output_byte[Output_index]);
		Output_index++;
		TxByteByUART(BPM);
	}

}

/**
 * @brief Function Name: TxByteByUART
 * @brief Description: This function takes the byte passed to it and uses
 * Timer A to transmit the byte. First the Bits_to_transmit variable typecasts
 * unsigned char value into an unsigned int, logically ORs this value with
 * 1 0000 0000, represented by BIT8 and finally multiplies this value by 2
 * by shifting the bits left once ( Bits_to_transmit = 1x xxxx xxx0 ). This is
 * done to added start and stop bits to the output. These bits will act as
 * a flag for the function to start transmitting data and to indicate that all
 * of the bits have been sent respectively.
 *
 * The value of capture/compare register 0 is synchronized with Timer A register
 * (TAR). This prevents the output bit latching from being out of sync with the
 * Timer A by setting the CCR0 register to the current state of the TAR before
 * each bit is transmitted. The bit_mask variable is initialized inside of a
 * for loop. The variable bit_mask is assigned the value 0000 0000 0000 0001
 * and compared with BITA, which is 0000 0100 0000 0000. This loop is
 * continued as loop as the value of bit_mask is less than BITA. Finally
 * bit_mask is shifted to the left by 1 and then the loop is executed. The
 * variable bit_mask is logically ANDed with Bits_to_transmit. This is a simple
 * comparison to determine if the current least significant bit (LSB) is a zero
 * or a one. When the value is not zero the output of port P1.1 is set high and
 * when the value is zero the port P1.1 is low.
 *
 * Between each bit comparison the value of CCR0 is set to CYCLES_PER_SYMBOL
 * plus the current value of CCR0. This is done to maintain the baud rate. The
 * baud rate of the Timer A based UART can be calculated as: SMCLK/CCR0
 * (500kHz/52 = 9600 BAUD).
 *
 * @param Input Parameters: Byte of data to be transmitted (TransmitByte)
 * @return Return Values: None
 * ****************************************************************************/
void TxByteByUART (unsigned char TransmitByte)
{
	unsigned int Bits_to_transmit, bit_mask;

	// Add start and stop bits.
	Bits_to_transmit = ((unsigned int) TransmitByte | BIT8) << 1;
	CCR0 = TAR;
	
	for (bit_mask = BIT0; bit_mask < BITA; bit_mask <<= 1)
	{
	  if ((bit_mask & Bits_to_transmit) != 0)
		P1OUT |=  TX_PIN;
	  else
		P1OUT &= ~(TX_PIN);

	  CCR0 += CYCLES_PER_SYMBOL;
	  CCTL0 = 0;
	  while ((CCTL0 & CCIFG) == 0);
	}
}

//****************************************************************************//
