
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

#define IDLE_STATE 0 // idle state after a reset
#define JOIN_STATE 1 // state where players can join game
#define PLAY_STATE 2 // state where game is in progress
#define END_STATE 3 // game over state

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

int i; // loop variable;
int count; // debounce counter

volatile int button_press = 0; // goes high after button was pressed

int state = 0; // state the game is currently in

char curr_id = 0;
char curr_code = 0;
char curr_pay = 0;

char player_ids[4]; // array of player IDs
int players = 0; // number of players in game
int joined; // flag to signal if player is already in game
char player_health[4];

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

// sets up button on base station
// button uses external interrupt 0 on pin 16
void buttonSetup(){
    TRISBbits.TRISB7 = 1; // set pin 16 as interrupt
    ConfigINT0(EXT_INT_ENABLE | RISING_EDGE_INT | EXT_INT_PRI_2);
    EnableINT0;
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

// Displays all players' health on TFT
void displayScoreBoard(){
    for(i=0;i<players;i++){
        tft_setCursor(0, 80+i*20);
        tft_setTextColor(ILI9340_YELLOW);
        tft_setTextSize(2);
        sprintf(buffer, "%s", "Player ");
        tft_writeString(buffer);

        tft_setCursor(80, 80+i*20);
        tft_setTextColor(ILI9340_YELLOW);
        tft_setTextSize(2);
        sprintf(buffer, "%d", player_ids[i]);
        tft_writeString(buffer);

        tft_setCursor(85, 80+i*20);
        tft_setTextColor(ILI9340_YELLOW);
        tft_setTextSize(2);
        sprintf(buffer, "%s", ": ");
        tft_writeString(buffer);

        tft_setCursor(100, 80+i*20);
        tft_setTextColor(ILI9340_RED);
        tft_setTextSize(2);
        sprintf(buffer, "%d", player_health[i]);
        tft_writeString(buffer);

    }
}

// signal players that game has ended
void sendEndGame(){
    for(i=0;i<players;i++){
        msg = (player_ids[i] << 6) | (0x11 << 4) | (player_health[i]); // signal each player that the game has ended
        // send the message
        nrf_pwrup();
        delay_ms(2);
        nrf_send_payload(&msg, 1);
        delay_ms(2);
        nrf_pwrdown();
        delay_ms(2);
    }
}

// Resets variables for next game
void reset(){
    curr_id = 0;
    curr_code = 0;
    curr_pay = 0;

    for(i=0;i<4;i++){
        player_ids[i] = 0;
        player_health[i] = 0;
    }
    players = 0; // number of players in game
    joined = 0; // flag to signal if player is already in game

    msg = 0;
    send = 0;
    receive = 0;
    received = 0;
    
    i = 0; // loop variable;
    count = 0; // debounce counter
    nrf_pwrdown();
}

// button was pressed
void __ISR(_EXTERNAL_0_VECTOR, ipl2) INT0Interrupt(){
   while(mPORTBReadBits(BIT_7)){
       count++;
       if(count > 1100){
       }
   }
   if(count > 1000){ // debounce button
       button_press = 1;
   }
   count = 0;
    mINT0ClearIntFlag();
}


static PT_THREAD(protothread_radio(struct pt *pt)) {
    PT_BEGIN(pt);
    while (1) {
        while(state == IDLE_STATE){ // reset state
            tft_setCursor(0, 160);
            tft_setTextColor(ILI9340_BLUE);
            tft_setTextSize(2);
            tft_writeString("State");
            
            tft_setCursor(0, 180);
            tft_setTextColor(ILI9340_GREEN);
            tft_setTextSize(2);
            sprintf(buffer, "%d", state);
            tft_writeString(buffer);
            if(button_press == 1){ // wait for a button to be pressed
                button_press = 0; // clear button press
                tft_fillScreen(ILI9340_BLACK);
                state = JOIN_STATE; // go to the join game state
            }
        }
    
        while(state == JOIN_STATE){//Letting people into the game
            nrf_pwrup();
            PT_YIELD_TIME_msec(2);
            nrf_rx_mode();
            PT_YIELD_TIME_msec(50);
            nrf_pwrdown();
            PT_YIELD_TIME_msec(2);
            
            
            if (received) {
                
//                tft_fillScreen(ILI9340_BLACK);
//                tft_setCursor(0, 60);
//                tft_setTextColor(ILI9340_MAGENTA);
//                tft_setTextSize(2);
//                tft_writeString("Sent");
//                nrf_read_reg(nrf24l01_STATUS, &status, 1);
//                
//                tft_setCursor(0, 80);
//                tft_setTextColor(ILI9340_YELLOW);
//                tft_setTextSize(2);
//                sprintf(buffer, "%X", receive);
//                tft_writeString(buffer);
//                received = 0;
//                nrf_flush_rx();
//                
//                
//                tft_setCursor(0, 100);
//                tft_setTextColor(ILI9340_YELLOW);
//                tft_setTextSize(2);
//                sprintf(buffer, "%X", curr_id);
//                tft_writeString(buffer);
//                
//                tft_setCursor(0, 120);
//                tft_setTextColor(ILI9340_YELLOW);
//                tft_setTextSize(2);
//                sprintf(buffer, "%X", curr_code);
//                tft_writeString(buffer);
//                
//                tft_setCursor(0, 140);
//                tft_setTextColor(ILI9340_YELLOW);
//                tft_setTextSize(2);
//                sprintf(buffer, "%X", curr_pay);
//                tft_writeString(buffer);

                receive = RX_payload[0];
                curr_id = (receive & 0xC0) >> 6;
                curr_code = (receive & 0x30) >> 4;
                curr_pay = (receive & 0x0F);
                //PT_YIELD_TIME_msec(1000);
                 
                receive = 0;
                for(i=0;i<4;i++){
                    if((curr_id == player_ids[i]) && (curr_id != 0)){ // check if player has already joined game
                        joined = 1;
                    }  
                }
                if(!joined){ // if the player has not already joined the game
                    for(i=0;i<4;i++){
                        if(player_ids[i] == 0){
                            player_ids[i] = curr_id; // put new id in array
                            player_health[i] = 8;
                            players += 1; // keep count of players in game
                            break;
                        }
                    }   
                }
                
                joined = 0;
                msg = (curr_id << 6) | (0x01 << 4); //Tell this guy he is in (in code is 01)
                curr_id = 0;
                nrf_pwrup();
                PT_YIELD_TIME_msec(2);
                nrf_send_payload(&msg, 1);
                PT_YIELD_TIME_msec(2);
                nrf_pwrdown();
                PT_YIELD_TIME_msec(2);
                
                // display players
                displayScoreBoard();
               
            }
            if(button_press == 1){ // if button was pressed go to play state
                button_press = 0; // clear the press
                for(i=0;i<players;i++){ // signal each player that game has begun
                    nrf_pwrdown();
                    PT_YIELD_TIME_msec(2);
                    msg = (players << 6) | (0x10 << 4); // send game start msg                    
                    nrf_pwrup();
                    PT_YIELD_TIME_msec(2);
                    nrf_send_payload(&msg, 1);
                    PT_YIELD_TIME_msec(2);
                }
                tft_fillScreen(ILI9340_BLACK);
                state = PLAY_STATE; // go to play state
            }
        }
        while(state == PLAY_STATE){ // While game is in progress
            tft_setCursor(0, 160);
            tft_setTextColor(ILI9340_BLUE);
            tft_setTextSize(2);
            tft_writeString("State");
            
            tft_setCursor(0, 180);
            tft_setTextColor(ILI9340_GREEN);
            tft_setTextSize(2);
            sprintf(buffer, "%d", state);
            tft_writeString(buffer);
            
            // put radio in receive mode periodically
            nrf_pwrup();
            PT_YIELD_TIME_msec(2);
            nrf_rx_mode();
            PT_YIELD_TIME_msec(50);
            nrf_pwrdown();
            PT_YIELD_TIME_msec(2);
            
            while(received){ // when data has been received
                receive = RX_payload[0]; // interpret payload
                curr_id = (receive & 0xC0) >> 6;
                curr_code = (receive & 0x30) >> 4;
                curr_pay = (receive & 0x0F);
                
                if(curr_code == 10){ // if the data is how much life a player has
                    for(i=1;i<players;i++){
                        if(player_ids[i] == curr_id){ // determine which player sent the payload 
                            player_health[i] = curr_pay;
                        }
                    }
                }
                // reset flags
                received = 0;
                receive = 0;
                displayScoreBoard(); // display updated health of players
            }            
            
            if(button_press == 1){
                button_press = 0;
                sendEndGame(); // signal the end of the game to all guns
                tft_fillScreen(ILI9340_BLACK);
                displayScoreBoard();
                state = END_STATE;
                
            }
        }
        
        while(state == END_STATE){ // After game has ended
            // If button is pressed go to IDLE or JOIN state
            tft_setCursor(0, 160);
            tft_setTextColor(ILI9340_BLUE);
            tft_setTextSize(2);
            tft_writeString("State");
            
            tft_setCursor(0, 180);
            tft_setTextColor(ILI9340_GREEN);
            tft_setTextSize(2);
            sprintf(buffer, "%d", state);
            tft_writeString(buffer);
            
            // signal the end of the game repeatedly incase a gun did not receive the message
            sendEndGame();
            if(button_press == 1){
                button_press = 0;
                reset(); // reset all variables for next game
                tft_fillScreen(ILI9340_BLACK);
                state = IDLE_STATE;
            }
        }
    }
    PT_END(pt);
} // timer thread

// === Main  ======================================================

void main(void) {
    INTEnableSystemMultiVectoredInt();
    PT_setup();
    buttonSetup();
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


