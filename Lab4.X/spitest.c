
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
// SDI -> MISO (RPA4) (pin 12)
// SDO -> MOSI (RPB2) (pin 6)
// IRQ -> extern interrupt 1 (RPB10) (pin 21)
// CSN -> RPB7 (I/O) (pin 16)
// CE -> RPB6 (I/O) (pin 15)


static char status;
static char config;
static char buffer[120];

int sending; // high when the radio is still sending data
char rf_spiwrite(unsigned char c){ // Transfer to SPI
    while (TxBufFullSPI2());
    WriteSPI2(c);
    while( !SPI2STATbits.SPIRBF); // check for complete transmit
    return ReadSPI2();
}

void init_SPI(){
    // Set up SPI2 to be active high, master, 8 bit mode, and ~4 Mhz CLK
    SpiChnOpen(2, SPI_OPEN_MSTEN | SPI_OPEN_MODE8 | SPI_OPEN_ON | SPI_OPEN_CKE_REV, 16);
    // Set SDI2 to pin 12
    PPSInput(3, SDI2, RPA4);
    // Set SDO2 to pin 9
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


void nrf_write_reg(char reg, char * data, char len){
    int i = 0;
    _csn = 0; // begin transmission
    status = rf_spiwrite(nrf24l01_W_REGISTER | reg); // send command to write reg
    for(i=0;i<len;i++){
        rf_spiwrite(data[i]); // write each char/byte to address reg
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
    _csn = 1; // end transmission
    
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

//Sets the power up bit and waits for the startup time, putting the radio in Standby-I mode
void nrf_pwrup(){
    nrf_read_reg(nrf24l01_CONFIG, &config, 1);
    config |= nrf24l01_CONFIG_PWR_UP;
    nrf_write_reg(nrf24l01_CONFIG, &config, 1);
    delay_ms(2);//Delay for power up time
}

//Clear the pwr_up bit, transitioning to power down mode
void nrf_pwrdown(){
    nrf_read_reg(nrf24l01_CONFIG, &config, 1);
    config &= ~(nrf24l01_CONFIG_PWR_UP);
    nrf_write_reg(nrf24l01_CONFIG, &config, 1);
    _ce = 0;
}

//Transitions to rx mode from standby mode
void nrf_rx_mode(){
    nrf_read_reg(nrf24l01_CONFIG, &config, 1);
    config |= nrf24l01_CONFIG_PRIM_RX;
    nrf_write_reg(nrf24l01_CONFIG, &config, 1);
    _ce = 1;
    delay_us(200); //Wait for transition to rx mode time
}

//Transitions to tx mode from standby mode
void nrf_tx_mode(){
    nrf_read_reg(nrf24l01_CONFIG, &config, 1);
    config &= ~(nrf24l01_CONFIG_PRIM_RX);
    nrf_write_reg(nrf24l01_CONFIG, &config, 1);
    _ce = 1;
    delay_us(200); //Wait for transition to tx mode time
}

void nrf_standby_mode(){
    _ce = 0;
}

// Sets the address to transmit to
// address: address to transmit to, 5 bytes, LSB first
// for auto_ack sent pipe 0 to same address on receiver
void nrf_set_TX_address(char * address){
    nrf_write_reg(nrf24l01_TX_ADDR, &address, 5);
}

// Sets the address on the receiving pipe specified (pipes 0 to 5)
// pipes 2-5 only allow LSB to be changed
// address: address to set pipe to, 1-5 bytes, LSB first
// pipe: pipe to set address of
void nrf_set_RX_address(char * address, int pipe){
    if ((pipe == 1) |(pipe == 0)){ // check if 5 byte address
        nrf_write_reg(nrf24l01_RX_ADDR_P0 + pipe, &address,	5);
    }
    else{
        nrf_write_reg(nrf24l01_RX_ADDR_P0 + pipe, &address,	1);
    }
}

// sets power of transmitter, possible values and definitions for them are
//  0dBm: nrf24l01_RF_SETUP_RF_PWR_0
// -6dBm: nrf24l01_RF_SETUP_RF_PWR_6
// -12dBm: nrf24l01_RF_SETUP_RF_PWR_12
// -18dBm: nrf24l01_RF_SETUP_RF_PWR_18
void nrf_set_transmit_pwr(char power){
    char setup;
    nrf_read_reg(nrf24l01_RF_SETUP, &setup, 1); // check value of setup register
    setup &= ~(nrf24l01_RF_SETUP_RF_PWR); // clear power bits in register
    setup |= power; // set power bits in register
    nrf_write_reg(nrf24l01_RF_SETUP, &setup, 1);  
}

// sets the rf data rate, possible values and definitoins for them are
// 250 kbps: nrf24l01_DR_LOW
// 1 Mbps: nrf24l01_DR_MED
// 2 Mbps: nrf24l01_DR_HIGH
void nrf_set_transmit_rate(char rate){
    char setup;
    ; // check value of setup register
    setup &= 0xd7;  // clear data rate bits in register
    setup |= rate; // set data rate bits in register
    nrf_write_reg(nrf24l01_RF_SETUP, &setup, 1); 
}

// Sends out a specified payload (in auto acknowledge mode by default)
// use after powering up radio, and setting address or other settings
void nrf_send_payload(char * data, int len){
    nrf_write_payload(data, len);
    nrf_tx_mode();
    while(sending); // wait until data sent interrupt triggers
    sending = 0;
    _ce = 0; // transition to standby II mode
    nrf_pwrdown(); // power down radio
    nrf_pwrup(); // power up radio to reenter standby I mode
}

void __ISR(_EXTERNAL_1_VECTOR, ipl2) INT1Handler(void){
    nrf_read_reg(nrf24l01_STATUS, &status, 1); // read the status register
    // check which type of interrupt occurred
    if (status & nrf24l01_STATUS_RX_DR){ // if data received
        
    }
    // if data sent or if acknowledge received when auto ack enabled
    else if(status & nrf24l01_STATUS_TX_DS){ 
        sending = 1; // signal main code that payload was sent
        status &= ~(nrf24l01_STATUS_TX_DS); // clear interrupt on radio
    }
    else{ // maximum number of retransmit attempts occurred
        
    }
    mINT1ClearIntFlag();
}

int main(void){
char * config = malloc(1); // will take value in config register   
char * address = malloc(5); // 5 byte address for testing
char * address_read = malloc(5); // data read from the address

INTEnableSystemMultiVectoredInt();

address[0] = 0xCE;
address[1] = 0xBE;

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
nrf_pwrup();//Go to standby

while(1){
    
    
}


//while(1){
//    // turn on power and set some random bit on config reg as a test testing
//    
//    nrf_read_reg(nrf24l01_CONFIG, &config, 1);
//    delay_ms(2);
//    delay_ms(1000);
//    nrf_pwrdown();
//    nrf_read_reg(nrf24l01_CONFIG, &config, 1);
//    delay_ms(1000);
//}
} // main
