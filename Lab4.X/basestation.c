
// graphics libraries
#include "config.h"
#include "tft_gfx.h"
#include "tft_master.h"
// need for rand function
#include <stdlib.h>
// serial stuff
#include <stdio.h>
#include <math.h>
// threading library

#define	SYS_FREQ 64000000 //40000000
#include "pt_cornell_TFT.h"

#include "plib.h"
#include "nrf24l01.h"

//#define	SYS_FREQ 64000000
#define dmaChn 0

#define spi_channel	1
// Use channel 2 since channel 1 is used by TFT display

#define spi_divider 10

static int timer_limit_1;
static int timer_limit_2;
static int timer_limit_3;

volatile unsigned char sine_table[256];

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_radio;

char send;
char receive;

int open = 1;
char curr_id = 0;
char curr_code = 0;
char curr_pay = 0;

char msg = 0;

char retry_num = nrf24l01_SETUP_RETR_ARC_15 | nrf24l01_SETUP_RETR_ARD_1000;
// Play game over sound

void playSound1() {

    DmaChnEnable(dmaChn);
    OpenTimer4(T4_ON | T4_SOURCE_INT | T4_PS_1_1, timer_limit_1);
    OpenTimer5(T5_ON | T5_SOURCE_INT | T5_PS_1_256, 50000);
    ConfigIntTimer5(T5_INT_ON | T5_INT_PRIOR_2);
    // Clear interrupt flag
    mT5ClearIntFlag();

}

// Play score decrease sound

void playSound2() {

    DmaChnEnable(dmaChn);
    OpenTimer4(T4_ON | T4_SOURCE_INT | T4_PS_1_1, timer_limit_2);
    OpenTimer5(T5_ON | T5_SOURCE_INT | T5_PS_1_256, 50000);
    ConfigIntTimer5(T5_INT_ON | T5_INT_PRIOR_2);
    // Clear interrupt flag
    mT5ClearIntFlag();

}

// Play score increase sound

void playSound3() {

    DmaChnEnable(dmaChn);
    OpenTimer4(T4_ON | T4_SOURCE_INT | T4_PS_1_1, timer_limit_3);
    OpenTimer5(T5_ON | T5_SOURCE_INT | T5_PS_1_256, 50000);
    ConfigIntTimer5(T5_INT_ON | T5_INT_PRIOR_2);
    // Clear interrupt flag
    mT5ClearIntFlag();
}

void __ISR(_TIMER_5_VECTOR, ipl2) T5HandlerISR(void){
    mT5ClearIntFlag();
    DmaChnDisable(dmaChn);
    CloseTimer5();
}

void radioSetup(){
    TX = 1;
    send = 0xBB;

    // Set outputs to CE and CSN
    TRIS_csn = 0;
    TRIS_ce = 0;

    init_SPI();

    // write the 5 byte address to pipe 1
    nrf_pwrup(); //Go to standby

    // set the payload width to 1 bytes
    payload_size = 1;
    nrf_write_reg(nrf24l01_RX_PW_P0, &payload_size, 1);
    nrf_write_reg(nrf24l01_RX_PW_P1, &payload_size, 1);
    nrf_write_reg(nrf24l01_RX_PW_P2, &payload_size, 1);
    nrf_write_reg(nrf24l01_RX_PW_P3, &payload_size, 1);
    nrf_write_reg(nrf24l01_RX_PW_P4, &payload_size, 1);
    nrf_write_reg(nrf24l01_RX_PW_P5, &payload_size, 1);
    nrf_write_reg(nrf24l01_SETUP_RETR, &retry_num, 1);

    nrf_flush_rx();
   
}

static PT_THREAD(protothread_radio(struct pt *pt)) {
    PT_BEGIN(pt);
    while (1) {
        while(open){//Letting people into the game
            nrf_pwrup();
            PT_YIELD_TIME_msec(2);
            nrf_rx_mode();
            PT_YIELD_TIME_msec(50);
            nrf_pwrdown();
            PT_YIELD_TIME_msec(2);
            receive = RX_payload[0];
            curr_id = (receive & 0xC0) >> 6;
            curr_code = (receive & 0x30) >> 4;
            curr_pay = (receive & 0x0F);
            
            if (received) {
                
                //tft_fillScreen(ILI9340_BLACK);
                tft_setCursor(0, 60);
                tft_setTextColor(ILI9340_MAGENTA);
                tft_setTextSize(2);
                tft_writeString("Sent");
                nrf_read_reg(nrf24l01_STATUS, &status, 1);
                
                tft_setCursor(0, 80);
                tft_setTextColor(ILI9340_YELLOW);
                tft_setTextSize(2);
                sprintf(buffer, "%X", receive);
                tft_writeString(buffer);
                received = 0;
                nrf_flush_rx();
                
                
                tft_setCursor(0, 100);
                tft_setTextColor(ILI9340_YELLOW);
                tft_setTextSize(2);
                sprintf(buffer, "%X", curr_id);
                tft_writeString(buffer);
                
                tft_setCursor(0, 120);
                tft_setTextColor(ILI9340_YELLOW);
                tft_setTextSize(2);
                sprintf(buffer, "%X", curr_code);
                tft_writeString(buffer);
                
                tft_setCursor(0, 140);
                tft_setTextColor(ILI9340_YELLOW);
                tft_setTextSize(2);
                sprintf(buffer, "%X", curr_pay);
                tft_writeString(buffer);

                
                //PT_YIELD_TIME_msec(1000);
                 
                receive = 0;
                
                msg = (curr_id << 6) | (0x01 << 4); //Tell this guy he is in (in code is 01)
                
                nrf_pwrup();
                PT_YIELD_TIME_msec(2);
                nrf_send_payload(&msg, 1);
                PT_YIELD_TIME_msec(2);
                nrf_pwrdown();
                PT_YIELD_TIME_msec(2);
               
            }
        }
        // if transmitter
        //PT_YIELD_TIME_msec(100);
        if (TX) {
            PT_YIELD_TIME_msec(1000);
     
        } else {
            nrf_rx_mode();
            while (1) {
                //LATAbits.LATA0 = 1;
                PT_YIELD_TIME_msec(1000);
                
                receive = RX_payload[0];
                if (received) {
                    //LATAbits.LATA0 = 0;
                    tft_fillScreen(ILI9340_BLACK);
                    //_LEDRED = 0;

                     PT_YIELD_TIME_msec(200);
                    tft_setCursor(0, 60);
                    tft_setTextColor(ILI9340_MAGENTA);
                    tft_setTextSize(2);
                    tft_writeString("Sent");
                    nrf_read_reg(nrf24l01_STATUS, &status, 1);
                    tft_setCursor(0, 300);
                    tft_setTextColor(ILI9340_YELLOW);
                    tft_setTextSize(2);
                    
                 
                    sprintf(buffer, "%X", receive);
                    tft_writeString(buffer);
                    received = 0;
                    receive = 0;
                    nrf_flush_rx();
                }else{
                     PT_YIELD_TIME_msec(100);
                }
            }
        }
    }
    PT_END(pt);
} // timer thread

// === Main  ======================================================

void main(void) {
    INTEnableSystemMultiVectoredInt();
    PT_setup();
    TRISAbits.TRISA0 = 0;
    LATAbits.LATA0 = 0;
    PT_INIT(&pt_radio);
    
    radioSetup();
   
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);
    //240x320 vertical display
    tft_setRotation(0); // Use tft_setRotation(1) for 320x240
    
    TX = 0;
    
    while (1) {
        PT_SCHEDULE(protothread_radio(&pt_radio));
    }
} // main

// === end  ======================================================


