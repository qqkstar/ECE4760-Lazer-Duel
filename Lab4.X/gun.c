
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

#include "laser_8khz_packed.h"
#include "pain_8khz_packed.h"
#include "dead_8khz_packed.h"
#include "ready_8khz_packed.h"

//#define	SYS_FREQ 64000000
#define SHOOT_LED BIT_0
#define LIFE_LED BIT_1
#define RECEIVER BIT_7
#define dmaChn 0

// Game states
#define IDLE_STATE 0 // idle state after a reset
#define JOIN_STATE 1 // state where players can join game
#define PLAY_STATE 2 // state where game is in progress
#define END_STATE 3 // game over state
#define WAIT_STATE 4 // waiting for game to start after joining

// Shift register latch
#define LATCH LATBbits.LATB4

#define spi_channel	1
// Use channel 2 since channel 1 is used by TFT display

// Devide clock for radio spi
#define spi_divider 10

// PIN Setup
// Latch Pin                         <-- RB4 (pin 11)
// ClockPin (SHCP)                   <-- SCK1(pin 25)
// DataPin (SPI1)                    <-- RB5 (pin 14)

static int alive = 1; // goes low for a few seconds after being hit


// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_timer, pt_radio;

volatile static char lives = 0xFF;
volatile static char life_cnt = 8;


static char receive; // received packet
static char ticket; // request to join packet
static char msg; // life count update packet
static char id = 1; // id of the gun

char curr_id = 0; // id code of packet
char curr_code = 0; // code for what packet is
char curr_pay = 0; // payload of packet

int state = 0; // what state the game is in (idle on reset)

int play_dead_sound = 0; // flag signalling the gun should play a hit sound

// volatiles for the stuff used in the ISR
volatile unsigned int i, j, packed, DAC_value; // lazer variables
char sound;
//volatile int CVRCON_setup; // stores the voltage ref config register after it is set up
// contains digit speech waveform packed so that
// low-order 4 bits is sample t and high order 4 bits is sample t+1

//Helper function to iterate through array and output voltage through CVREF
void playSound(unsigned char* AllDigits, int size) {
    j = i>>1;
    if (~(i & 1)) packed = AllDigits[j] ;
    if (i & 1) DAC_value = packed>>4 ; // upper 4 bits
    else  DAC_value = packed & 0x0f ; // lower 4 bits
    CVRCON = CVRCON_setup | DAC_value ;
    i++ ;
    if (j>size) i = 0;
}

// sets up spi for the shift register
void SPI_setup() {
    TRISBbits.TRISB5 = 0; // configure pin RB as an output  
    PPSOutput(2, RPB5, SDO1); // map SDO1 to RB5

}

// transfers date to the shift register to display lives
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

//play shooting sound
void playLaser(){
        i = 0;  //clear 
        j = 0;
        packed = 0;
        DAC_value = 0;
        sound = 'l';
        // set up the Vref pin and use as a DAC
        // enable module| eanble output | use low range output | use internal reference | desired step
        CVREFOpen( CVREF_ENABLE | CVREF_OUTPUT_ENABLE | CVREF_RANGE_LOW | CVREF_SOURCE_AVDD | CVREF_STEP_0 );
        // And read back setup from CVRCON for speed later
        // 0x8060 is enabled with output enabled, Vdd ref, and 0-0.6(Vdd) range
        CVRCON_setup = CVRCON; //CVRCON = 0x8060 from Tahmid http://tahmidmc.blogspot.com/

        // Set up timer2 on,  interrupts, internal clock, prescalar 1, toggle rate
        // For voice synth run at 8 kHz
        OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, 5000);
        // set up the timer interrupt with a priority of 2
        ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
        //ConfigIntTimer5(T5_INT_ON | T5_INT_PRIOR_2);
        mT2ClearIntFlag(); // and clear the interrupt flag
}   

// Play losing life sound
void playPain(){
        i = 0;  //clear 
        j = 0;
        packed = 0;
        DAC_value = 0;
        sound = 'p';
        // set up the Vref pin and use as a DAC
        // enable module| eanble output | use low range output | use internal reference | desired step
        CVREFOpen( CVREF_ENABLE | CVREF_OUTPUT_ENABLE | CVREF_RANGE_LOW | CVREF_SOURCE_AVDD | CVREF_STEP_0 );
        // And read back setup from CVRCON for speed later
        // 0x8060 is enabled with output enabled, Vdd ref, and 0-0.6(Vdd) range
        CVRCON_setup = CVRCON; //CVRCON = 0x8060 from Tahmid http://tahmidmc.blogspot.com/

        // Set up timer2 on,  interrupts, internal clock, prescalar 1, toggle rate
        // For voice synth run at 8 kHz
        OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, 3000);
        // set up the timer interrupt with a priority of 2
        ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
        //ConfigIntTimer5(T5_INT_ON | T5_INT_PRIOR_2);
        mT2ClearIntFlag(); // and clear the interrupt flag
   
}

// Play dead sound
void playDead(){
        i = 0;  //clear 
        j = 0;
        packed = 0;
        DAC_value = 0;
        sound = 'd';
        // set up the Vref pin and use as a DAC
        // enable module| eanble output | use low range output | use internal reference | desired step
        CVREFOpen( CVREF_ENABLE | CVREF_OUTPUT_ENABLE | CVREF_RANGE_LOW | CVREF_SOURCE_AVDD | CVREF_STEP_0 );
        // And read back setup from CVRCON for speed later
        // 0x8060 is enabled with output enabled, Vdd ref, and 0-0.6(Vdd) range
        CVRCON_setup = CVRCON; //CVRCON = 0x8060 from Tahmid http://tahmidmc.blogspot.com/

        // Set up timer2 on,  interrupts, internal clock, prescalar 1, toggle rate
        // For voice synth run at 8 kHz
        OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, 5000);
        // set up the timer interrupt with a priority of 2
        ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
        //ConfigIntTimer5(T5_INT_ON | T5_INT_PRIOR_2);
        mT2ClearIntFlag(); // and clear the interrupt flag
}

// Play ready sound
void playReady(){
        i = 0;  //clear 
        j = 0;
        packed = 0;
        DAC_value = 0;
        sound = 'r';
        // set up the Vref pin and use as a DAC
        // enable module| eanble output | use low range output | use internal reference | desired step
        CVREFOpen( CVREF_ENABLE | CVREF_OUTPUT_ENABLE | CVREF_RANGE_LOW | CVREF_SOURCE_AVDD | CVREF_STEP_0 );
        // And read back setup from CVRCON for speed later
        // 0x8060 is enabled with output enabled, Vdd ref, and 0-0.6(Vdd) range
        CVRCON_setup = CVRCON; //CVRCON = 0x8060 from Tahmid http://tahmidmc.blogspot.com/

        // Set up timer2 on,  interrupts, internal clock, prescalar 1, toggle rate
        // For voice synth run at 8 kHz
        OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, 6000);
        // set up the timer interrupt with a priority of 2
        ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
        //ConfigIntTimer5(T5_INT_ON | T5_INT_PRIOR_2);
        mT2ClearIntFlag(); // and clear the interrupt flag
}

// parses a packet to make it easier to interpret
void parsePacket() {
    receive = RX_payload[0]; // check what message was received
    curr_id = (receive & 0xC0) >> 6; // id is the first to bits
    curr_code = (receive & 0x30) >> 4; // code is the second two bits
    curr_pay = (receive & 0x0F); // payload is last four bits
}

// configures the radio
void radioSetup() {
    // Set outputs to CE and CSN
    TRIS_csn = 0;
    TRIS_ce = 0;

	// sets up spi for the radio
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

	// disable auto ack
    char autoack = nrf24l01_EN_AA_ENAA_NONE;
    nrf_write_reg(nrf24l01_EN_AA, &autoack, 1);
    char disable_retry = nrf24l01_SETUP_RETR_ARC_0;
    nrf_write_reg(nrf24l01_SETUP_RETR, &disable_retry, 1);
    nrf_flush_rx(); // clear the rx fifo

}

// configures the gun hardware
void gunSetup() {
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

	// configures int0 for the ir detector
    ConfigINT0(EXT_INT_ENABLE | FALLING_EDGE_INT | EXT_INT_PRI_2);
    mINT0ClearIntFlag();
    EnableINT0;
    mINT0ClearIntFlag();
    
	// configures the trigger and life and shoot leds
    mPORTASetPinsDigitalIn(BIT_1);
    mPORTBSetPinsDigitalOut(SHOOT_LED | LIFE_LED); //Shoot and life LEDs
    mPORTBClearBits(SHOOT_LED);
    mPORTBSetBits(LIFE_LED);
    //mPORTBSetPinsDigitalIn(RECEIVER);

	// setup the spi for the shift register and display lives leds
    SPI_setup();
    SPI1_transfer(lives);
    // round-robin scheduler for threads
}

// gun thread
static PT_THREAD(protothread_timer(struct pt *pt)) {
    PT_BEGIN(pt);

    while (1) {
        // idle state before attempting to join game
        while (state == IDLE_STATE) {
            PT_YIELD_TIME_msec(2);
            if (mPORTAReadBits(BIT_1)) { // wait for trigger press
                state = JOIN_STATE; // go to join game state
                nrf_pwrdown();
                nrf_pwrup();
                PT_YIELD_TIME_msec(2);
            }
        }

        // state where gun attempts to join game
        while (state == JOIN_STATE) {
            PT_YIELD_TIME_msec(100);
            if (mPORTAReadBits(BIT_1)) { // wait for trigger press
                state = WAIT_STATE; // go to join game state
            }
        }

		// state where gun waits for game to start after joining
        while (state == WAIT_STATE) {
            PT_YIELD_TIME_msec(100);
        }

		// state game is played in
        while (state == PLAY_STATE) {
            CVRCON = 0; // turn off cvref
            PT_YIELD_TIME_msec(10);
            SPI1_transfer(lives); // display player's health
            if (alive) { // check if player has not been hit               
                if (mPORTAReadBits(BIT_1)) { // check if player has shot the gun
                    OC1RS = 842; // duty cycle of IR emitter (shoot gun)
                    mPORTBSetBits(SHOOT_LED); // blink LED to signal the player has shot
                    CVREFOpen(CVREF_ENABLE | CVREF_OUTPUT_ENABLE | CVREF_RANGE_LOW | CVREF_SOURCE_AVDD | CVREF_STEP_0);
                    playLaser(); // play a shooting sound
                    PT_YIELD_TIME_msec(500); // send the shot as a pulse
                    CloseTimer2();
                    OC1RS = 0; // turn off emitter
                    mPORTBClearBits(SHOOT_LED); // turn off shoot LED
                    CVRCON = 0;
                } else { // If the player hasn't shot yield to radio
                    OC1RS = 0;
                    PT_YIELD_TIME_msec(200);
                }
            } else { // if the player was shot
                mPORTBClearBits(LIFE_LED); // turn off life LED to signal player was shot
                CVREFOpen(CVREF_ENABLE | CVREF_OUTPUT_ENABLE | CVREF_RANGE_LOW | CVREF_SOURCE_AVDD | CVREF_STEP_0);
                playPain(); // play a hit sound
                PT_YIELD_TIME_msec(500);
                CloseTimer2();
                if (lives != 0) { // if player hasn't run out of lives
                    PT_YIELD_TIME_msec(200);
                    mPORTBSetBits(LIFE_LED); // turn back on life LED to signal player is alive
                    alive = 1;
                    PT_YIELD_TIME_msec(20);
                    CVRCON = 0;
                    PT_YIELD_TIME_msec(100);
                } else { // if player is out of lives
                    state = END_STATE;
                }
            }
        }

		// state after game ends
        while (state == END_STATE) { // if player has dies or game has ended 
            // blink the shoot LED to signal game over
            mPORTBClearBits(LIFE_LED); // turn off life LED at end of game
			// blink shoot led to signal game over
            mPORTBSetBits(SHOOT_LED); 
            PT_YIELD_TIME_msec(500);
            mPORTBClearBits(SHOOT_LED);
            PT_YIELD_TIME_msec(500);
			// play dead sound once
            if (play_dead_sound==0) {
                playDead();
                PT_YIELD_TIME_msec(1300);
                CloseTimer2();
                play_dead_sound = 1;
            }

        }
        // END WHILE(1)
        PT_YIELD_TIME_msec(500);
    } // timer thread
    PT_END(pt);
}

// radio thread
static PT_THREAD(protothread_radio(struct pt * pt)) {
    PT_BEGIN(pt);
    while (1) {
		
		// idle state before attempting to join game 
        while (state == IDLE_STATE) {
            PT_YIELD_TIME_msec(2);
        }
		
		// state where gun attempts to join game
        while (state == JOIN_STATE) {
			// turn on the radio
            nrf_pwrdown();
            nrf_pwrup();
            PT_YIELD_TIME_msec(2);
            mPORTBSetBits(SHOOT_LED); // turn on shoot led
			// send id with request to join game
            ticket = ((id << 6)); 
            PT_YIELD_TIME_msec(2);
            nrf_send_payload(&ticket, 1);
            PT_YIELD_TIME_msec(10);
           
        }
		
		// state where gun waits for game to start after joining
        while (state == WAIT_STATE) {
			// put radio in rx mode to wait for game start packet
            nrf_pwrdown();
            nrf_pwrup();
            PT_YIELD_TIME_msec(5);
            nrf_flush_rx();
            nrf_flush_tx();
            PT_YIELD_TIME_msec(5);
            nrf_rx_mode();
			
			// blink led to signal gun is in join state
            PT_YIELD_TIME_msec(100);
            mPORTBSetBits(SHOOT_LED);
            PT_YIELD_TIME_msec(100);
            mPORTBClearBits(SHOOT_LED);
            PT_YIELD_TIME_msec(100);
			
            if (received) { // if a packet is received
                parsePacket();
                if (curr_code == 0b10) { // if the payload is a game start for the right gun
                    playReady(); // play sound to signal start of game
					// set up variables for game start
                    PT_YIELD_TIME_msec(1300);
                    CloseTimer2();
                    lives = 0xFF;
                    life_cnt = 8;
                    alive = 1;
                    received = 0; // clear flag
                    state = PLAY_STATE;
                } else { // if the payload wasn't for a game start do clear the flag
                    received = 0;
                }
            }
        }
		
		// state game is played in
        while (state == PLAY_STATE) {
            // send current life to base station
            nrf_pwrdown();
            nrf_pwrup();
            PT_YIELD_TIME_msec(2);
            error = 0;
            msg = ((id << 6) | (0b10 << 4) | life_cnt);
            nrf_send_payload(&msg, 1);
            PT_YIELD_TIME_msec(10);
			
			// go into receive mode to check for game over message
            nrf_pwrdown();
            nrf_pwrup();
            PT_YIELD_TIME_msec(2);
            nrf_rx_mode(); // see if end game message was sent
            PT_YIELD_TIME_msec(200);
			
            if (received) { // if a message was received
                parsePacket();
                if (curr_code == 0b11) { // if the message is a game over message
                    nrf_pwrdown();
                    state = END_STATE;
                }
                received = 0; // clear flag
            }
        }
		
		// state after game ends
        while (state == END_STATE) {
			// keep transmitting current life
            nrf_pwrdown();
            nrf_pwrup();
            PT_YIELD_TIME_msec(2);
            error = 0;
            msg = ((id << 6) | (0b10 << 4) | life_cnt);
            nrf_send_payload(&msg, 1);
            PT_YIELD_TIME_msec(200);
        }
    }
    PT_END(pt);
} // timer thread

// === Main  ======================================================

void main(void) {
	// set up interrupts
    INTEnableSystemMultiVectoredInt();
	// set up protothreads
    PT_setup();
    PT_INIT(&pt_timer);
    PT_INIT(&pt_radio);
	// set up shift register
    TRISBbits.TRISB4 = 0;

	// set up radio
    radioSetup();
	// set up gun
    gunSetup();

    while (1) {
        PT_SCHEDULE(protothread_timer(&pt_timer));
        PT_SCHEDULE(protothread_radio(&pt_radio));
    }
} // main

// === end  ======================================================

// Interrupt for IR sensor
void __ISR(_EXTERNAL_0_VECTOR, ipl2) INT0Interrupt() {
    if(alive){ // if the player was alive when shot
        alive = 0; // set player as dead
		// decrement lives
        lives = lives << 1;
        life_cnt--;
    }
    mINT0ClearIntFlag(); // clear flag

}

// Sound interrupt
void __ISR(_TIMER_2_VECTOR, ipl2) Timer2Handler(void)
{
    // clear the interrupt flag
    mT2ClearIntFlag();
    if (sound == 'l') {
        playSound(AllDigits_laser, sizeof(AllDigits_laser));
    }
    else if (sound == 'p') {
        playSound(AllDigits_pain, sizeof(AllDigits_pain));
    }
    else if (sound == 'd') {
        playSound(AllDigits_dead, sizeof(AllDigits_dead));
    }
    else if (sound == 'r') {
        playSound(AllDigits_ready, sizeof(AllDigits_ready));
    }
}