// graphics libraries
#include "config.h"
#include "tft_master.h"
#include "tft_gfx.h"
// need for rand function
#include <stdlib.h>
// serial stuff
#include <stdio.h>
#include <math.h>
// threading library

#define	SYS_FREQ 64000000 //40000000
#include "pt_cornell_TFT.h"

#include "plib.h"

#define	SYS_FREQ 64000000
#define SHOOT_LED BIT_0
#define LIFE_LED BIT_1
#define RECEIVER BIT_6
#define dmaChn 0


#define LATCH LATBbits.LATB4

#define spi_channel	1
    // Use channel 2 since channel 1 is used by TFT display

#define spi_divider 10

// PIN Setup for shift reg
// Latch Pin                         <-- RB4 (pin 11)
// ClockPin (SHCP)                   <-- SCK1(pin 25)
// DataPin (SPI1)                    <-- RB5 (pin 14)

// volatiles for the stuff used in the ISR
volatile unsigned int i, j, packed, DAC_value; // lazer variables
char sound;
//volatile int CVRCON_setup; // stores the voltage ref config register after it is set up
// contains digit speech waveform packed so that
// low-order 4 bits is sample t and high order 4 bits is sample t+1

#include "laser_8khz_packed.h"
#include "pain_8khz_packed.h"
#include "dead_8khz_packed.h"
#include "ready_8khz_packed.h"

static struct pt pt3, pt_input, pt_output, pt_DMA_output ;
// turn threads 1 and 2 on/off and set thread timing
int sys_time_seconds ;
int alive = 1;

void playSound(unsigned char* AllDigits, int size) {
    j = i>>1;
    if (~(i & 1)) packed = AllDigits[j] ;
    if (i & 1) DAC_value = packed>>4 ; // upper 4 bits
    else  DAC_value = packed & 0x0f ; // lower 4 bits
    CVRCON = CVRCON_setup | DAC_value ;
    i++ ;
    if (j>size) i = 0;
}

// Timer 2 interrupt handler ///////
// ipl2 means "interrupt priority level 2"
// ASM output is 47 instructions for the ISR
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

static int timer_limit_1;
static int timer_limit_2;
static int timer_limit_3;

volatile unsigned char sine_table[256];

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_timer, pt_rpm, pt_pid ;

volatile static int lives = 0xFF;

void SPI_setup() 
{
  TRISBbits.TRISB4 = 0;   // configure pin RB as an output  
  PPSOutput(2, RPB5, SDO1);	// map SDO1 to RA1

}

void SPI1_transfer( int data)
{                 
    SpiChnOpen(spi_channel, SPI_OPEN_MSTEN | SPI_OPEN_MODE8 | SPI_OPEN_DISSDI | SPI_OPEN_CKE_REV, spi_divider);
    LATCH = 0;     // set pin RB0 low / disable latch
    while (TxBufFullSPI1());	// ensure buffer is free before writing
    WriteSPI1(data);			// send the data through SPI
    while (SPI1STATbits.SPIBUSY); // blocking wait for end of transaction
    LATCH = 1;     // set pin RB0 high / enable latch
    ReadSPI1();
    //SpiChnClose(spi_channel);
    CloseSPI1();
}

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

// Play score decrease sound
void playSound2(){

    DmaChnEnable(dmaChn);
    OpenTimer4(T4_ON | T4_SOURCE_INT | T4_PS_1_1, timer_limit_2);
    OpenTimer5(T5_ON | T5_SOURCE_INT | T5_PS_1_256, 50000);
    ConfigIntTimer5(T5_INT_ON | T5_INT_PRIOR_2);
    // Clear interrupt flag
    mT5ClearIntFlag();
    
}

// Play score increase sound
void playSound3(){

    DmaChnEnable(dmaChn);
    OpenTimer4(T4_ON | T4_SOURCE_INT | T4_PS_1_1, timer_limit_3);
    OpenTimer5(T5_ON | T5_SOURCE_INT | T5_PS_1_256, 50000);
    ConfigIntTimer5(T5_INT_ON | T5_INT_PRIOR_2);
    // Clear interrupt flag
    mT5ClearIntFlag();
}

void __ISR(_TIMER_5_VECTOR, ipl2) T5Handler(void){
    mT3ClearIntFlag();
    CVREFClose();
    //DmaChnDisable(dmaChn);
    CloseTimer2();
    CloseTimer5();
}

static PT_THREAD (protothread_timer(struct pt *pt))
{
    PT_BEGIN(pt);
     
      while(1) {
          
          //Trigger pressed, fire IR
          if(alive == 1){
            if(mPORTAReadBits(BIT_1)){
                OC1RS = 842;
                mPORTBSetBits(SHOOT_LED);
                //CVREFOpen( CVREF_ENABLE | CVREF_OUTPUT_ENABLE | CVREF_RANGE_LOW | CVREF_SOURCE_AVDD | CVREF_STEP_0 );
               playReady();
                PT_YIELD_TIME_msec(1100);
                OC1RS = 0;  
                mPORTBClearBits(SHOOT_LED);
                //CVREFOpen(CVREF_DISABLE | CVREF_OUTPUT_ENABLE | CVREF_RANGE_LOW | CVREF_SOURCE_AVDD | CVREF_STEP_0 );
                //CVREFClose();
                CVRCON = 0;
                PT_YIELD_TIME_msec(200);
                
            }else{
                OC1RS = 0;
            }
          }else{
              mPORTBClearBits(LIFE_LED);
             CVRCON = 0;
             PT_YIELD_TIME_msec(10);
              SPI1_transfer(lives);
              CVREFOpen( CVREF_ENABLE | CVREF_OUTPUT_ENABLE | CVREF_RANGE_LOW | CVREF_SOURCE_AVDD | CVREF_STEP_0 );
              playSound2();
              
              PT_YIELD_TIME_msec(2000);
              playSound3();
              PT_YIELD_TIME_msec(200);
              mPORTBSetBits(LIFE_LED);
              alive = 1;
              //CVREFClose();
              //CVREFOpen(CVREF_DISABLE | CVREF_OUTPUT_ENABLE | CVREF_RANGE_LOW | CVREF_SOURCE_AVDD | CVREF_STEP_0 );
              CVRCON = 0;
              PT_YIELD_TIME_msec(100);
               
              
          }

      } // END WHILE(1)
  PT_END(pt);
} // timer thread

// === Main  ======================================================
void main(void) {

  // === config threads ==========
  // turns OFF UART support and debugger pin
  PT_setup();


  // === setup system wide interrupts  ========
  INTEnableSystemMultiVectoredInt();

  //OpenTimer2(T2_ON  | T2_SOURCE_INT | T2_PS_1_256, 65535);
  // set up the timer interrupt with a priority of 2
  //ConfigIntTimer2(T2_INT_OFF | T2_INT_PRIOR_2);
  
  
  //period of 64000 should make the timer overflow frequency 38 kHz, the desired PWM frequency
  OpenTimer3(T3_ON | T3_SOURCE_INT | T3_PS_1_1, 1684);
  ConfigIntTimer2(T3_INT_OFF | T3_INT_PRIOR_2);
  
  //OC STUFF
  // set pulse to go high at 1/4 of the timer period and drop again at 1/2 the timer period
  //ONLY TIMER 2 AND 3 CAN BE USED WITH OC. CAN CHANGE THE PROTOTHREADS TIMER COUNTER
  //TO LET US USE 2 HERE AND TIMER 1 FOR PROTOTHREADS AND 45 FOR RPM
  OpenOC1(OC_ON | OC_TIMER3_SRC | OC_PWM_FAULT_PIN_DISABLE , 0, 0);
  // OC1 is PPS group 1, map to RPA0 (pin 2)
  PPSOutput(1, RPA0, OC1);
  OC1RS = 842;
  
  PT_INIT(&pt_timer);
 
  
   timer_limit_1 = SYS_FREQ/(256*900);
    timer_limit_2 = SYS_FREQ/(256*200);
    timer_limit_3 = SYS_FREQ/(256*300);
    

    // set up the Vref pin and use as a DAC
    // enable module| eanble output | use low range output | use internal reference | desired step
    //CVREFOpen( CVREF_ENABLE | CVREF_OUTPUT_ENABLE | CVREF_RANGE_LOW | CVREF_SOURCE_AVDD | CVREF_STEP_0 );
    // And read back setup from CVRCON for speed later
    // 0x8060 is enabled with output enabled, Vdd ref, and 0-0.6(Vdd) range

    int i;
    for (i=0; i <256; i++){
        sine_table[i] = (signed char) (8.0 * (sin((float)i*6.283/(float)256) + sin((float)(4.5*i*6.283)/(float)256)));
        sine_table[i] = (sine_table[i] & 0x0F) | 0x8060;
    }

    // Open the desired DMA channel.
    // We enable the AUTO option, we'll keep repeating the sam transfer over and over.
    //DmaChnOpen(dmaChn, 0, DMA_OPEN_AUTO);

    // set the transfer parameters: source & destination address, source & destination size, number of bytes per event
    // Setting the last parameter to one makes the DMA output one byte/interrupt
    //DmaChnSetTxfer(dmaChn, sine_table, (void*)&CVRCON, 256, 1, 1);

    // set the transfer event control: what event is to start the DMA transfer
    // In this case, timer2
    //DmaChnSetEventControl(dmaChn, DMA_EV_START_IRQ(_TIMER_4_IRQ));
    
  // init the display
  tft_init_hw();
  tft_begin();
  tft_fillScreen(ILI9340_BLACK);
  //240x320 vertical display
  tft_setRotation(0); // Use tft_setRotation(1) for 320x240
  
  ConfigINT0(EXT_INT_ENABLE | FALLING_EDGE_INT |EXT_INT_PRI_2);
  EnableINT0;
  
  mPORTASetPinsDigitalIn(BIT_1);
  mPORTBSetPinsDigitalOut(SHOOT_LED| LIFE_LED); //Shoot and life LEDs
  mPORTBClearBits(SHOOT_LED);
  mPORTBSetBits(LIFE_LED);
  mPORTBSetPinsDigitalIn(RECEIVER);
  
  INTEnableSystemMultiVectoredInt();
  
 SPI_setup();
 SPI1_transfer(lives);
  // round-robin scheduler for threads
  while (1){
      PT_SCHEDULE(protothread_timer(&pt_timer));
      }
  } // main

// === end  ======================================================

void __ISR(_EXTERNAL_0_VECTOR, ipl2) INT0Interrupt() 
{ 
   int cnt = 0;
   //Debounce
   while(cnt<1000){
       if(mPORTBReadBits(RECEIVER) == 0){
           cnt++;
       }else{
           break;
       }
   }
   if(cnt == 1000){
        alive = 0;
        lives = lives << 1;
   }else{
       alive = 1;
   }
   mINT0ClearIntFlag(); 
       
} 

