/*********************************************************************
 *
 *  This example generates a sine wave using the CVref output
 *  Refer also to http://tahmidmc.blogspot.com/
 *  CVref is a 4-bit DAC
 *  Pin 25 on the MicrostickII
 *
 * From ref manual: in low range
 *  Output Z at output code 0 (about zero volts) is about 500 ohms
 *  Output Z at output code 15 (about 2 volts) is about 10k ohms
 *********************************************************************
 * Bruce Land, Cornell University
 * May 30, 2014
 ********************************************************************/
// all peripheral library includes
#include <plib.h>
#include <math.h>

// Configuration Bit settings
// SYSCLK = 40 MHz (8MHz Crystal/ FPLLIDIV * FPLLMUL / FPLLODIV)
// PBCLK = 40 MHz
// Primary Osc w/PLL (XT+,HS+,EC+PLL)
// WDT OFF
// Other options are don't care
//
#pragma config FNOSC = FRCPLL, POSCMOD = HS, FPLLIDIV = DIV_2, FPLLMUL = MUL_20, FPBDIV = DIV_1, FPLLODIV = DIV_2
#pragma config FWDTEN = OFF
// frequency we're running at
#define	SYS_FREQ 40000000

// volatiles for the stuff used in the ISR
volatile unsigned int i, j, packed, DAC_value; // voice variables
volatile int CVRCON_setup; // stores the voltage ref config register after it is set up
// contains digit speech waveform packed so that
// low-order 4 bits is sample t and high order 4 bits is sample t+1
//#include "AllDigits_packed.h"
#include "laser_8khz_packed.h"

// Timer 2 interrupt handler ///////
// ipl2 means "interrupt priority level 2"
// ASM output is 47 instructions for the ISR
void __ISR(_TIMER_2_VECTOR, ipl2) Timer2Handler(void)
{
    // clear the interrupt flag
    mT2ClearIntFlag();
    // do the Direct Digital Synthesis
    j = i>>1;
    if (~(i & 1)) packed = AllDigits[j] ;
    if (i & 1) DAC_value = packed>>4 ; // upper 4 bits
    else  DAC_value = packed & 0x0f ; // lower 4 bits
    CVRCON = CVRCON_setup | DAC_value ;
    i++ ;
    if (j>sizeof(AllDigits)) i = 0;
}

// main ////////////////////////////
int main(void)
{
	// Configure the device for maximum performance but do not change the PBDIV
	// Given the options, this function will change the flash wait states, RAM
	// wait state and enable prefetch cache but will not change the PBDIV.
	// The PBDIV value is already set via the pragma FPBDIV option above..
	SYSTEMConfig(SYS_FREQ, SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);

        // set up the Vref pin and use as a DAC
        // enable module| eanble output | use low range output | use internal reference | desired step
        CVREFOpen( CVREF_ENABLE | CVREF_OUTPUT_ENABLE | CVREF_RANGE_LOW | CVREF_SOURCE_AVDD | CVREF_STEP_0 );
        // And read back setup from CVRCON for speed later
        // 0x8060 is enabled with output enabled, Vdd ref, and 0-0.6(Vdd) range
        CVRCON_setup = CVRCON; //CVRCON = 0x8060 from Tahmid http://tahmidmc.blogspot.com/

        // Set up timer2 on,  interrupts, internal clock, prescalar 1, toggle rate
        // For voice synth run at 8 kHz
        OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, 2500);

        // set up the timer interrupt with a priority of 2
         ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
        mT2ClearIntFlag(); // and clear the interrupt flag

        // setup system wide interrupts  ///
        INTEnableSystemMultiVectoredInt();

        i = 0 ;
        // set up i/o port pin 
       // mPORTAClearBits(BIT_0);		//Clear bits to ensure light is off.
       // mPORTASetPinsDigitalOut(BIT_0);    //Set port as output

	while(1)
	{		
           // toggle a bit for ISR perfromance measure
           // mPORTAToggleBits(BIT_0);
            // shows that the ISR stops MAIN for 1.5 microSec
            // every 10 microSec
 	}
	return 0;
}






