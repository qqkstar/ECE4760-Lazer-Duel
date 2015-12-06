
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

void __ISR(_TIMER_5_VECTOR, ipl2) T5HandlerISR(void) {
    mT5ClearIntFlag();
    DmaChnDisable(dmaChn);
    CloseTimer5();
}

// sets up button on base station
// button uses external interrupt 0 on pin 16

void buttonSetup() {
    TRISBbits.TRISB7 = 1; // set pin 16 as interrupt
    ConfigINT0(EXT_INT_ENABLE | RISING_EDGE_INT | EXT_INT_PRI_2);
    EnableINT0;
}

void radioSetup() {
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
    //nrf_write_reg(nrf24l01_SETUP_RETR, &retry_num, 1);

    char autoack = nrf24l01_EN_AA_ENAA_NONE;
    nrf_write_reg(nrf24l01_EN_AA, &autoack, 1);
    char disable_retry = nrf24l01_SETUP_RETR_ARC_0;
    nrf_write_reg(nrf24l01_SETUP_RETR, &disable_retry, 1);
    nrf_flush_rx();

}

// disassembles a packet
int parsePacket() {
    receive = RX_payload[0]; // check what message was received
    curr_id = (receive & 0xC0) >> 6;
    curr_code = (receive & 0x30) >> 4;
    curr_pay = (receive & 0x0F);
    
    if(curr_pay > 8 | curr_id > 3 | curr_code > 3){
        return 0;
    }else{
        return 1;
    }
}

// Displays all players' health on TFT
void displayScoreBoard() {
    
    for (i = 0; i < players; i++) {
        tft_setCursor(0, 80 + i * 20);
        tft_setTextColor(ILI9340_YELLOW);
        tft_setTextSize(2);
        sprintf(buffer, "%s", "Player ");
        tft_writeString(buffer);

        tft_setCursor(80, 80 + i * 20);
        tft_setTextColor(ILI9340_YELLOW);
        tft_setTextSize(2);
        sprintf(buffer, "%d", player_ids[i]);
        tft_writeString(buffer);

        tft_setCursor(90, 80 + i * 20);
        tft_setTextColor(ILI9340_YELLOW);
        tft_setTextSize(2);
        sprintf(buffer, "%s", ": ");
        tft_writeString(buffer);

        tft_setCursor(100, 80 + i * 20);
        tft_setTextColor(ILI9340_RED);
        tft_setTextSize(2);
        sprintf(buffer, "%d", player_health[i]);
        tft_writeString(buffer);

    }
}

// signal players that game has ended

void sendEndGame() {
    for (i = 0; i < players; i++) {
        msg = (player_ids[i] << 6) | (0b11 << 4) | (player_health[i]); // signal each player that the game has ended
        // send the message
        nrf_pwrup();
        delay_ms(2);
        nrf_send_payload(&msg, 1);
        delay_ms(10);
        nrf_pwrdown();
    }
}

void displayWinner(){
    static int k;
    static int compare = 0;
    static int winner = 0;
    for(k=0;k<players;k++){
        if(player_health[k] > compare){
            compare = player_health[k];
            winner = k;
        }        
        
    }
    tft_setCursor(0, 220);
    tft_setTextColor(ILI9340_YELLOW);
    tft_setTextSize(2);
    sprintf(buffer, "%s", "Player ");
    tft_writeString(buffer);

    tft_setCursor(80, 220);
    tft_setTextColor(ILI9340_YELLOW);
    tft_setTextSize(2);
    sprintf(buffer, "%d", player_ids[winner]);
    tft_writeString(buffer);

    tft_setCursor(100, 220);
    tft_setTextColor(ILI9340_YELLOW);
    tft_setTextSize(2);
    sprintf(buffer, "%s", "Wins");
    tft_writeString(buffer);        
}

// Resets variables for next game

void reset() {
    curr_id = 0;
    curr_code = 0;
    curr_pay = 0;

    for (i = 0; i < 4; i++) {
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

void __ISR(_EXTERNAL_0_VECTOR, ipl2) INT0Interrupt() {
    while (mPORTBReadBits(BIT_7)) {
        count++;
//        if (count > 750) {
//        }
    }
    if (count > 700) { // debounce button
        button_press = 1;
    }
    count = 0;
    mINT0ClearIntFlag();
}

static PT_THREAD(protothread_radio(struct pt *pt)) {
    PT_BEGIN(pt);
    while (1) {
        while (state == IDLE_STATE) { // reset state
            tft_setCursor(0, 0);
            tft_setTextColor(ILI9340_GREEN);
            tft_setTextSize(2);
            tft_writeString("Push to begin");
            if (button_press == 1) { // wait for a button to be pressed
                button_press = 0; // clear button press
                tft_fillScreen(ILI9340_BLACK);
                state = JOIN_STATE; // go to the join game state
                nrf_flush_rx();
                nrf_pwrup();
                PT_YIELD_TIME_msec(2);
            }
        }

        while (state == JOIN_STATE) {//Letting people into the game
            tft_setCursor(0, 0);
            tft_setTextColor(ILI9340_GREEN);
            tft_setTextSize(2);
            tft_writeString("Waiting for players...");
            nrf_rx_mode();
            PT_YIELD_TIME_msec(200);
            if (received) {                       
                static int proper;
                proper = parsePacket();
                received = 0;
                if(proper){
                    nrf_flush_rx();
                    nrf_flush_tx();
                    for (i = 0; i < 4; i++) {
                        if ((curr_id == player_ids[i]) && (curr_id != 0)) { // check if player has already joined game
                            joined = 1;
                        }
                    }

                    if (!joined) { // if the player has not already joined the game
                        for (i = 0; i < 4; i++) {
                            if (player_ids[i] == 0) {
                                player_ids[i] = curr_id; // put new id in array
                                player_health[i] = 8;
                                players += 1; // keep count of players in game
                                break;
                            }
                        }
                    }
                    nrf_flush_rx();
                    displayScoreBoard();
                }
            }

            joined = 0;


            // display players
           
            
            if (button_press == 1) { // if button was pressed go to play state
                nrf_flush_tx();
                nrf_flush_rx();
                button_press = 0; // clear the press
                error = 0;
                nrf_pwrdown();
                PT_YIELD_TIME_msec(2);
                msg = (0b10 << 4); // send game start msg
                
                nrf_pwrup();
                PT_YIELD_TIME_msec(5);
                nrf_flush_tx();
                nrf_flush_rx();
                PT_YIELD_TIME_msec(5);
                nrf_send_payload(&msg, 1);
                PT_YIELD_TIME_msec(20);
                tft_fillScreen(ILI9340_BLACK);
                state = PLAY_STATE; // go to play state
            }
        }
        
        while (state == PLAY_STATE) { // While game is in progress
            tft_setCursor(0, 0);
            tft_setTextColor(ILI9340_GREEN);
            tft_setTextSize(2);
            tft_writeString("Game start");
            
            // put radio in receive mode periodically
            nrf_pwrup();
            PT_YIELD_TIME_msec(2);
            nrf_rx_mode();
            PT_YIELD_TIME_msec(100);
            nrf_pwrdown();
            PT_YIELD_TIME_msec(2);

            while (received) { // when data has been received
                parsePacket();
                if (curr_code == 0b10) { // if the data is how much life a player has
                    for (i = 0; i < players; i++) {
                        if (player_ids[i] == curr_id) { // determine which player sent the payload 
                            player_health[i] = curr_pay; // update the player's health
                        }
                    }
                }
                // reset flags
                received = 0;
                receive = 0;
                tft_fillRoundRect(0, 80, 250, 100, 1, ILI9340_BLACK);
                displayScoreBoard(); // display updated health of players
            }
           
            static int k;
            static int j;
            for(k=0;k<players;k++){
                if(player_health[k] == 0){
                    j++;
                }
            }
            // end the game if all but one player dies
            if(j==(players-1)){
                button_press = 0;
                sendEndGame(); // signal the end of the game to all guns
                tft_fillScreen(ILI9340_BLACK);
                displayScoreBoard();
                displayWinner();
                state = END_STATE;
            }

            if (button_press == 1) {
                button_press = 0;
                sendEndGame(); // signal the end of the game to all guns
                tft_fillScreen(ILI9340_BLACK);
                displayScoreBoard();
                displayWinner();
                state = END_STATE;

            }
        }

        while (state == END_STATE) { // After game has ended
            // If button is pressed go to IDLE or JOIN state
            tft_setCursor(0, 0);
            tft_setTextColor(ILI9340_GREEN);
            tft_setTextSize(2);
            tft_writeString("GAME OVER");

            // signal the end of the game repeatedly incase a gun did not receive the message
            sendEndGame();            
            if (button_press == 1) {
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
    //reset();
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
    
     for (i = 0; i < 4; i++) {
        player_ids[i] = 0;
        player_health[i] = 0;
    }
    
    while (1) {
        PT_SCHEDULE(protothread_radio(&pt_radio));
    }
} // main

// === end  ======================================================


