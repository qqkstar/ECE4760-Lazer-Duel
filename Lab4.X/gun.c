
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
#define SHOOT_LED BIT_0
#define LIFE_LED BIT_1
#define RECEIVER BIT_7
#define dmaChn 0

#define IDLE_STATE 0 // idle state after a reset
#define JOIN_STATE 1 // state where players can join game
#define PLAY_STATE 2 // state where game is in progress
#define END_STATE 3 // game over state
#define WAIT_STATE 4 // waiting for game to start after joining

#define LATCH LATBbits.LATB4

#define spi_channel	1
// Use channel 2 since channel 1 is used by TFT display

#define spi_divider 10

// PIN Setup
// Latch Pin                         <-- RB4 (pin 11)
// ClockPin (SHCP)                   <-- SCK1(pin 25)
// DataPin (SPI1)                    <-- RB5 (pin 14)

static int alive = 1; // goes low for a few seconds after being hit


static int timer_limit_1;
static int timer_limit_2;
static int timer_limit_3;

volatile unsigned char sine_table[256];

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_timer, pt_radio;

volatile static char lives = 0xFF;
volatile static char life_cnt = 8;

static char send;
static char receive;
static char ticket;
static char msg;
static int joined = 0;
static char id = 2;
static char idle = 1;

char curr_id = 0;
char curr_code = 0;
char curr_pay = 0;

int state = 0;

void SPI_setup() {
    TRISBbits.TRISB5 = 0; // configure pin RB as an output  
    PPSOutput(2, RPB5, SDO1); // map SDO1 to RA1

}

void SPI1_transfer(int data) {
    SpiChnOpen(spi_channel, SPI_OPEN_MSTEN | SPI_OPEN_MODE8 | SPI_OPEN_DISSDI | SPI_OPEN_CKE_REV, spi_divider);
    LATCH = 0; // set pin RB0 low / disable latch
    while (TxBufFullSPI1()); // ensure buffer is free before writing
    WriteSPI1(data); // send the data through SPI
    while (SPI1STATbits.SPIBUSY); // blocking wait for end of transaction
    LATCH = 1; // set pin RB0 high / enable latch
    ReadSPI1();
    //SpiChnClose(spi_channel);
    CloseSPI1();
}

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

void __ISR(_TIMER_5_VECTOR, ipl2) T5HandlerISR(void) {
    mT5ClearIntFlag();
    DmaChnDisable(dmaChn);
    //CloseTimer2();
    CloseTimer5();
}

void parsePacket() {
    receive = RX_payload[0]; // check what message was received
    curr_id = (receive & 0xC0) >> 6;
    curr_code = (receive & 0x30) >> 4;
    curr_pay = (receive & 0x0F);
}

void radioSetup() {
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

    // Disable auto ack
    char disable_ack = nrf24l01_EN_AA_ENAA_NONE;
    //nrf_write_reg(nrf24l01_EN_AA, &disable_ack, 1);

    //_TRIS_LEDRED = 0;
    //_TRIS_LEDYELLOW = 0;
    //_LEDRED = 0;
    //_LEDYELLOW = 0;
    nrf_flush_rx();

}

void gunSetup() {



    OpenTimer4(T4_ON | T4_SOURCE_INT | T4_PS_1_256, 65535);
    // set up the timer interrupt with a priority of 2
    ConfigIntTimer4(T4_INT_OFF | T4_INT_PRIOR_2);

    //period of 64000 should make the timer overflow frequency 38 kHz, the desired PWM frequency
    OpenTimer3(T3_ON | T3_SOURCE_INT | T3_PS_1_1, 1684);
    ConfigIntTimer3(T3_INT_OFF | T3_INT_PRIOR_2);

    //OC STUFF
    // set pulse to go high at 1/4 of the timer period and drop again at 1/2 the timer period
    //ONLY TIMER 2 AND 3 CAN BE USED WITH OC. CAN CHANGE THE PROTOTHREADS TIMER COUNTER
    //TO LET US USE 2 HERE AND TIMER 1 FOR PROTOTHREADS AND 45 FOR RPM
    OpenOC1(OC_ON | OC_TIMER3_SRC | OC_PWM_FAULT_PIN_DISABLE, 0, 0);
    // OC1 is PPS group 1, map to RPA0 (pin 2)
    PPSOutput(1, RPA0, OC1);
    OC1RS = 0;



    timer_limit_1 = SYS_FREQ / (256 * 900);
    timer_limit_2 = SYS_FREQ / (256 * 200);
    timer_limit_3 = SYS_FREQ / (256 * 300);


    // set up the Vref pin and use as a DAC
    // enable module| eanble output | use low range output | use internal reference | desired step
    //CVREFOpen( CVREF_ENABLE | CVREF_OUTPUT_ENABLE | CVREF_RANGE_LOW | CVREF_SOURCE_AVDD | CVREF_STEP_0 );
    // And read back setup from CVRCON for speed later
    // 0x8060 is enabled with output enabled, Vdd ref, and 0-0.6(Vdd) range
    int i;
    for (i = 0; i < 256; i++) {
        sine_table[i] = (signed char) (8.0 * (sin((float) i * 6.283 / (float) 256) + sin((float) (4.5 * i * 6.283) / (float) 256)));
        sine_table[i] = (sine_table[i] & 0x0F) | 0x8060;
    }

    // Open the desired DMA channel.
    // We enable the AUTO option, we'll keep repeating the sam transfer over and over.
    DmaChnOpen(dmaChn, 0, DMA_OPEN_AUTO);

    // set the transfer parameters: source & destination address, source & destination size, number of bytes per event
    // Setting the last parameter to one makes the DMA output one byte/interrupt
    DmaChnSetTxfer(dmaChn, sine_table, (void*) &CVRCON, 256, 1, 1);

    // set the transfer event control: what event is to start the DMA transfer
    // In this case, timer2
    DmaChnSetEventControl(dmaChn, DMA_EV_START_IRQ(_TIMER_4_IRQ));

    ConfigINT0(EXT_INT_ENABLE | FALLING_EDGE_INT | EXT_INT_PRI_2);
    EnableINT0;

    mPORTASetPinsDigitalIn(BIT_1);
    mPORTBSetPinsDigitalOut(SHOOT_LED | LIFE_LED); //Shoot and life LEDs
    mPORTBClearBits(SHOOT_LED);
    mPORTBSetBits(LIFE_LED);
    //mPORTBSetPinsDigitalIn(RECEIVER);

    SPI_setup();
    SPI1_transfer(lives);
    // round-robin scheduler for threads
}

static PT_THREAD(protothread_timer(struct pt *pt)) {
    PT_BEGIN(pt);

    while (1) {
        // idle state before attempting to join game
        while (state == IDLE_STATE) {
            //mPORTBClearBits(LIFE_LED); // turn off life LED
            PT_YIELD_TIME_msec(2);
            if (mPORTAReadBits(BIT_1)) { // wait for trigger press
                state = JOIN_STATE; // go to join game state
            }
        }

        // state where gun attempts to join game before game start
        while (state == JOIN_STATE) {
            PT_YIELD_TIME_msec(100);
        }

        while (state == WAIT_STATE) {
            PT_YIELD_TIME_msec(100);
        }


        while (state == PLAY_STATE) {
            mPORTBClearBits(LIFE_LED);
            delay_ms(500);
            mPORTBSetBits(LIFE_LED);
            delay_ms(500);
        }
    }

    // END WHILE(1)
    PT_END(pt);
} // timer thread

static PT_THREAD(protothread_radio(struct pt *pt)) {
    PT_BEGIN(pt);
    while (1) {
        while (state == IDLE_STATE) {
            PT_YIELD_TIME_msec(2);
        }

        while (state == JOIN_STATE) {
            mPORTBSetBits(SHOOT_LED);
            ticket = (id << 6); // send id with request to join game
            nrf_pwrup();
            PT_YIELD_TIME_msec(2);
            nrf_send_payload(&ticket, 1);
            nrf_pwrdown();
            nrf_pwrup();
            PT_YIELD_TIME_msec(2);
            nrf_rx_mode(); // wait for confirmation of join
            PT_YIELD_TIME_msec(2000);
            nrf_pwrdown();
            receive = RX_payload[0];
            curr_id = (receive & 0xC0) >> 6;
            curr_code = (receive & 0x30) >> 4;
            curr_pay = (receive & 0x0F);
            if (received == 1) { // if confirmation was received 
                received = 0;
                if (((receive & 0xC0) >> 6) == id) { // check if confirmation was for correct gun
                    state = WAIT_STATE; // wait for game to begin
                    nrf_pwrup();
                } else {

                }
                nrf_flush_rx();
            }
        }

        while (state == WAIT_STATE) {
            nrf_rx_mode(); // wait for confirmation of join
            mPORTBSetBits(SHOOT_LED);
            PT_YIELD_TIME_msec(500);
            mPORTBClearBits(SHOOT_LED);
            PT_YIELD_TIME_msec(500);
            if (received) {
                parsePacket();
                if (curr_code == 0b10) { // if the payload is a game start for the right gun
                    playSound1(); // play sound to signal start of game
                    lives = 0xFF;
                    life_cnt = 8;
                    alive = 1;
                    received = 0;
                    state = PLAY_STATE;
                } else { // if the payload wasn't for a game start do clear the flag
                    
                    received = 0;
                }
            }
        }
        
        while(state == PLAY_STATE){
            mPORTBSetBits(SHOOT_LED);
            PT_YIELD_TIME_msec(2000);
            mPORTBClearBits(SHOOT_LED);
            PT_YIELD_TIME_msec(2000);
        }
        
        while (0) {
            msg = ((id << 6) | life_cnt);
            nrf_send_payload(&msg, 1);
            send = send + 1;
            PT_YIELD_TIME_msec(2000);

        }
    }
    PT_END(pt);
} // timer thread

// === Main  ======================================================

void main(void) {
    INTEnableSystemMultiVectoredInt();
    PT_setup();


    PT_INIT(&pt_timer);
    PT_INIT(&pt_radio);
    TRISBbits.TRISB4 = 0;

    radioSetup();
    gunSetup();

    while (1) {
        PT_SCHEDULE(protothread_timer(&pt_timer));
        PT_SCHEDULE(protothread_radio(&pt_radio));
    }
} // main

// === end  ======================================================

// Interrupt for IR sensor

void __ISR(_EXTERNAL_0_VECTOR, ipl2) INT0Interrupt() {
    alive = 0;
    lives = lives << 1;
    life_cnt--;

    DisableINT0;
    mINT0ClearIntFlag();

}

