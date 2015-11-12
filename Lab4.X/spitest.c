
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
#include <stdlib.h>
// protoThreads environment
#include "pt_cornell_TFT.h"
#include "nrf24l01.h"
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
// SDO -> MOSI (RPB2) (pin 6)
// IRQ -> extern interrupt 1 (RPB10) (pin 21)
// CSN -> RPB7 (I/O) (pin 16)
// CE -> RPB6 (I/O) (pin 15)


//char reg_read[5]; // register used for reading a single register
//char payload_read[32]; // register used for reading payload (find length of payload using R_RX_PL_WID)
char * status;
char buffer[120];
void rf_spiwrite(unsigned char c){ // Transfer to SPI
    while (TxBufFullSPI2());
    WriteSPI2(c);
    while (SPI2STATbits.SPIBUSY); // wait for it to end of transaction
}

void init_SPI(){
    // Set up SPI2 to be active high, master, 8 bit mode, and ~4 Mhz CLK
    SpiChnOpen(2, SPI_OPEN_MSTEN | SPI_OPEN_MODE8 | SPI_OPEN_ON | SPI_OPEN_CKE_REV, 16);
    // Set SDI2 to pin 24
    PPSInput(3, SDI2, RPB13);
    // Set SDO2 to pin 6
    PPSOutput(3, RPA2, SDO2);
    // Set external interrupt 1 to pin 21
    PPSInput(4, INT1, RPB10);
}

// Read a register from the nrf24l01
// reg is the array to read, len is the length of data expected to be received (1-5 bytes)
// NOTE: only address 0 and 1 registers use 5 bytes all others use 1 byte 
// NOTE: writing or reading payload is done using a specific command
void nrf_read_reg(char reg, char * buff, int len){
    //char reg_read[5]; // register used for reading a single register
    int i = 0;
    _csn = 0; // begin transmission
    rf_spiwrite(nrf24l01_W_REGISTER | reg); // send command to read register
    status = (char *)ReadSPI2(); // get status register back when sending command
    for(i=0;i<len;i++){
        rf_spiwrite(nrf24l01_SEND_CLOCK); // send clock pulse to continue receiving data
        buff[i] = ReadSPI2(); // get the data from the register starting with LSB
    }
    _csn = 1; // end transmission
  
}

//char* nrf_read_payload(char reg){
//    int i = 0;
//    char * width = malloc(5); // width of payload to read in first char
//    nrf_read_reg(nrf24l01_R_REGISTER_WID, width, 1); // get the size of the payload
//    
//    _csn = 0; // begin transmission
//    // send command to read payload register NOTE: payload deleted after reading
//    rf_spiwrite(nrf24l01_R_RX_PAYLOAD);
//    status = (char *)ReadSPI2(); // get back status register when sending command
//    for(i=0;i<width[i];i++){
//        rf_spiwrite(nrf24l01_SEND_CLOCK); // send a clock pulse while reading
//        payload_read[i] = ReadSPI2(); // get byte of payload LSB first
//    }
//    free(width);
//    _csn = 1; // end transmission
//    
//    return payload_read; // return array containing payload
//}

int main(void){
char * status = malloc(1); // status register as of latest read or write
char * config = malloc(1); // will take value in config register   

// Set outputs to CE and CSN
TRIS_csn = 0;
TRIS_ce = 0;

// turn on power and set some random bit on config reg as a test testing
_csn = 0;
rf_spiwrite(nrf24l01_W_REGISTER | nrf24l01_CONFIG);
rf_spiwrite(nrf24l01_CONFIG_PRIM_RX | nrf24l01_CONFIG_PWR_UP | nrf24l01_CONFIG_CRCO);
_csn = 1;

nrf_read_reg(nrf24l01_CONFIG,config, 1); // read value in config register

 tft_init_hw();
 tft_begin();
 tft_fillScreen(ILI9340_BLACK);
 //240x320 vertical display
 tft_setRotation(0); // Use tft_setRotation(1) for 320x240
  
while(1){
    if (config[0] == (nrf24l01_CONFIG_PRIM_RX | nrf24l01_CONFIG_PWR_UP | nrf24l01_CONFIG_CRCO)){
        //_ce = 1;  // toggle ce pin high to signal successful read
        //delay_ms(100);
        _ce = 0;
    }else{
        _ce ^= 1;
        delay_ms(100);
    }
    tft_setCursor(0, 220);
    tft_setTextColor(ILI9340_MAGENTA); 
    tft_setTextSize(2);
    tft_writeString("Config: ");

    tft_fillRoundRect(0,240, 200, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
    tft_setCursor(0, 240);
    tft_setTextColor(ILI9340_CYAN); 
    tft_setTextSize(2);
    sprintf(buffer,"%d", config[0]);
    tft_writeString(buffer);
}
//
//delay_ms(1);
//    while(1){
//        _csn = 0;
//        rf_spiwrite(nrf24l01_R_REGISTER | nrf24l01_CONFIG);
//        rf_spiwrite(nrf24l01_SEND_CLOCK);
//        _csn = 1;
//        delay_ms(1);
//    }



} // main
