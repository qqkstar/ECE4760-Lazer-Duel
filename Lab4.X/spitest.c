
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


static char status;
static char buffer[120];
char rf_spiwrite(unsigned char c){ // Transfer to SPI
    while (TxBufFullSPI2());
    WriteSPI2(c);
    while( !SPI2STATbits.SPIRBF); // check for complete transmit
    return SPI2BUF;
}

void init_SPI(){
    // Set up SPI2 to be active high, master, 8 bit mode, and ~4 Mhz CLK
    SpiChnOpen(2, SPI_OPEN_MSTEN | SPI_OPEN_MODE8 | SPI_OPEN_ON | SPI_OPEN_CKE_REV, 16);
    // Set SDI2 to pin 24
    PPSInput(3, SDI2, RPA4);
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
    status = rf_spiwrite(nrf24l01_R_REGISTER | reg); // send command to read register
    
    for(i=0;i<len;i++){
        buff[i] = rf_spiwrite(nrf24l01_SEND_CLOCK); // send clock pulse to continue receiving data
        //buff[i] = (char)ReadSPI2(); // get the data from the register starting with LSB
    }
    _csn = 1; // end transmission
}


// writes data to a register
// data: array of chars to be written (1-5 bytes)
// len: amount of chars/bytes to be written
// NOTE: can only be used in power down or standby mode
// NOTE: writing address or payload done with specific command
void nrf_write_reg(char reg, char data){
    _csn = 0; // begin transmission
    status = rf_spiwrite(nrf24l01_W_REGISTER | reg); // send the command to write reg
    rf_spiwrite(data);
    _csn = 1; // end transmission
}


// sets address of a pipe
// pipe: RX pipe address should be set for (6 for TX pipe)
// data: address to be written to pipe (LSB first in array)
// len: amount of bytes to be written as address
// NOTE: RX pipes 0 and 1 and TX pipe accept 5 bytes, the rest accept 1 byte
// NOTE: for pipes 2-5 only writes to LSB
// NOTE: can only be used in power down or standby mode
// NOT TESTED YET currently returns negative of what it should I think
void nrf_write_address(char pipe, char * address, char len){
    int i = 0;
    char reg;  // register of pipe
    reg = nrf24l01_RX_ADDR_P0 + pipe;
    
    _csn = 0; // begin transmission
    status = rf_spiwrite(nrf24l01_W_REGISTER | reg); // send command to write reg
    for(i=0;i<len;i++){
        rf_spiwrite(address[i]); // write each char/byte to address reg
    }
    _csn = 1; // end transmission
}


// Write a payload to be sent over the radio
// data: array of chars to be sent (1-32 chars/bytes)
// len: amount of chars in array/bytes to be sent
// NOT TESTED YET
void nrf_write_payload(char * data, char len){
    int i = 0;
    
    _csn = 0; // begin transmission
    status = rf_spiwrite(nrf24l01_W_TX_PAYLOAD); // send the command to write the payload
    for(i=0;i<len;i++){
    rf_spiwrite(data[i]); // write each char/byte to tx payload one at a time
    }
    _csn = 0; // end transmission
    
}

// should read the payload into a buffer NOT TESTED YET
void nrf_read_payload(char reg, char * buff){
    int i = 0;
    char * width = malloc(1); // width of payload to read in first char
    nrf_read_reg(nrf24l01_R_REGISTER_WID, width, 1); // get the size of the payload
    
    _csn = 0; // begin transmission
    status = rf_spiwrite(nrf24l01_R_REGISTER | reg); // send command to read payload
    for(i=0;i<width[0];i++){
        buff[i] = rf_spiwrite(nrf24l01_SEND_CLOCK);
    }
    _csn = 1; // end transmission
    free(width);
}

int main(void){
char * config = malloc(1); // will take value in config register   
char address[5]; // 5 byte address for testing
char address_read[5]; // data read from the address
int address_data; // the last two bytes of the address read from the register

// address = 0xC2C2C2C200
address[0] = 0xCE;
address[1] = 0xCE;
address[2] = 0x00;
address[3] = 0x00;
address[4] = 0x00;

// Set outputs to CE and CSN
TRIS_csn = 0;
TRIS_ce = 0;

 init_SPI();
 tft_init_hw();
 tft_begin();
 tft_fillScreen(ILI9340_BLACK);
 //240x320 vertical display
 tft_setRotation(0); // Use tft_setRotation(1) for 320x240
 
 // write the 5 byte address to pipe 1
 nrf_write_address(1, address, 5);
 
while(1){
    // turn on power and set some random bit on config reg as a test testing
  

    nrf_read_reg(nrf24l01_RX_ADDR_P1,address_read,5); // read value in address register
    
    address_data = address_read[1] + address_read[0];
    
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
    
     tft_setCursor(0, 180);
    tft_setTextColor(ILI9340_MAGENTA); 
    tft_setTextSize(2);
    tft_writeString("Status: ");

    tft_fillRoundRect(0,200, 200, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
    tft_setCursor(0, 200);
    tft_setTextColor(ILI9340_CYAN); 
    tft_setTextSize(2);
    sprintf(buffer,"%d", status);
    tft_writeString(buffer);
    
    tft_setCursor(0, 260);
    tft_setTextColor(ILI9340_MAGENTA); 
    tft_setTextSize(2);
    tft_writeString("Address: ");

    tft_fillRoundRect(0,280, 200, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
    tft_setCursor(0, 240);
    tft_setTextColor(ILI9340_CYAN); 
    tft_setTextSize(2);
    sprintf(buffer,"%d", address_data);
    tft_writeString(buffer);
    
    delay_ms(100);

}
} // main
