
// i/o names 
#define _SUPPRESS_PLIB_WARNING 1
#include <plib.h>

// frequency we're running at
#define	SYS_FREQ 64000000
// serial stuff
#include <stdio.h>
#include "config.h"
#include "tft_master.h"
#include "tft_gfx.h"
// protoThreads environment

// PIN Setup
// Latch Pin                         <-- RB0 (pin 4)
// ClockPin (SHCP)                   <-- SCK1(pin 25)
// DataPin (SPI1)                    <-- RA1 (pin 3)


int led_out = 0b11111111;

void setup() 
{
  ANSELBbits.ANSB0 = 0;   // sets pin RB0 as digital
  TRISBbits.TRISB0 = 0;   // configure pin RB as an output 
  
  PPSOutput(2, RPA1, SDO1);	// map SDO1 to RA1

  
    //#define config1 SPI_MODE16_ON | SPI_CKE_ON | MASTER_ENABLE_ON
  //	/*	FRAME_ENABLE_OFF
  //	 *	ENABLE_SDO_PIN		-> SPI Output pin enabled
  //	 *	SPI_MODE16_ON		-> 16-bit SPI mode
  //	 *	SPI_SMP_OFF			-> Sample at middle of data output time
  //	 *	SPI_CKE_ON			-> Output data changes on transition from active clock
  //	 *							to idle clock state
  //	 *	SLAVE_ENABLE_OFF	-> Manual SW control of SS
  //	 *	MASTER_ENABLE_ON	-> Master mode enable
  //	 */
  //#define config2 SPI_ENABLE
  //	/*	SPI_ENABLE	-> Enable SPI module
  //	 */
  ////	OpenSPI2(config1, config2);
  //	// see pg 193 in plib reference

  #define spi_channel	1
      // Use channel 2 since channel 1 is used by TFT display

  #define spi_brg	0
      // Divider = 2 * (spi_brg + 1)
      // Divide by 2 to get SPI clock of FPBDIV/2 -> max SPI clock

  //	SpiChnSetBrg(spi_channel, spi_brg);
      // see pg 203 in plib reference

  //////

  //////

  #define spi_divider 6
  /* Unlike OpenSPIx(), config for SpiChnOpen describes the non-default
   * settings. eg for OpenSPI2(), use SPI_SMP_OFF (default) to sample
   * at the middle of the data output, use SPI_SMP_ON to sample at end. For
   * SpiChnOpen, using SPICON_SMP as a parameter will use the non-default
   * SPI_SMP_ON setting.
   */
  //#define config SPI_OPEN_MSTEN | SPI_OPEN_MODE8 | SPI_OPEN_DISSDI | SPI_OPEN_CKE_REV
      /*	SPI_OPEN_MSTEN		-> Master mode enable
       *	SPI_OPEN_MODE16		-> 16-bit SPI mode
       *	SPI_OPEN_DISSDI		-> Disable SDI pin since PIC32 to DAC is a
       *							master-to-slave	only communication
       *	SPI_OPEN_CKE_REV	-> Output data changes on transition from active
       *							clock to idle clock state
       */

  SpiChnOpen(spi_channel, SPI_OPEN_MSTEN | SPI_OPEN_MODE8 | SPI_OPEN_DISSDI | SPI_OPEN_CKE_REV, spi_divider);

}
 
 
void SPI1_transfer( int data)
{                 
    LATBbits.LATB0 = 0;     // set pin RB0 low / disable latch
    while (TxBufFullSPI1());	// ensure buffer is free before writing
    WriteSPI1(data);			// send the data through SPI
    while (SPI1STATbits.SPIBUSY); // blocking wait for end of transaction
    LATBbits.LATB0 = 1;     // set pin RB0 high / enable latch
    ReadSPI1();
}

int main(void)
{
    char data = 0x10;
    char MSB;
    setup();
    while(1){
        if((data & 0x80) == 0x80){
            MSB = 1;
        }
        else{
            MSB = 0;
        }
        data = data << 1;
        data += MSB;
        SPI1_transfer( data );
        delay_ms(100);
    }
}