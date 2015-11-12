
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
#include "pt_cornell_TFT.h"

// === thread structures ============================================
// semaphores for controlling two threads
// for guarding the UART and for allowing stread blink control
// thread control structs
// note that UART input and output are threads
static struct pt pt3, pt_input, pt_output, pt_DMA_output ;
// turn threads 1 and 2 on/off and set thread timing
int sys_time_seconds ;


// PIN Setup
// SCK -> SCK1 (pin 26)
// SDI -> MISO (RPB13) (pin 24)
// SDO -> MOSI (RPB 11) (pin 22)
// IRQ -> extern interrupt 1 (RPB10) (pin 21)
// CSN -> RPB7 (I/O) (pin 16))
// CE -> RPB6 (I/O) (pin 15))

#define _csn         LATBbits.LATB7
#define TRIS_csn     TRISBbits.TRISB7

#define _ce         LATBbits.LATB8
#define TRIS_ce     TRISBbits.TRISB8

void rf_spiwrite(unsigned char c){ // Transfer to SPI
    while (TxBufFullSPI2());
    WriteSPI2(c);
    while (SPI2STATbits.SPIBUSY); // wait for it to end of transaction
}

int main(void){

//OpenSPI2(FRAME_ENABLE_OFF | ENABLE_SDO_PIN | SPI_MODE32_OFF | SPI_MODE16_OFF | SPI_MODE8_ON | SLAVE_ENABLE_OFF | MASTER_ENABLE_ON | CLK_POL_ACTIVE_HIGH | SPI_SMP_OFF | SPI_CKE_ON);
// Set up SPI2 to be active high, master, 8 bit mode, and ~4 Mhz CLK
SpiChnOpen(2, SPI_OPEN_MSTEN | SPI_OPEN_MODE8 | SPI_OPEN_ON | SPI_OPEN_CKE_REV , 16);
// Set SDI2 to pin 24
PPSInput(3, RPB13, SDI2);
// Set SDO2 to pin 22
PPSOutput(3, RPB11, SDO2);
// Set external interrupt 1 to pin 21
PPSOutput(4, RPB10, INT1);
// Set outputs to CE and CSN
TRIS_csn = 0;
TRIS_ce = 0;

    while(1){
        _csn = 0;
        rf_spiwrite(0xf0); // Transfer to SPI
        _csn = 1;
    }

} // main