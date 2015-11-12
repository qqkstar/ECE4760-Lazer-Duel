/*
 * File:        TFT_test_BRL4.c
 * Author:      Bruce Land
 * Adapted from:
 *              main.c by
 * Author:      Syed Tahmid Mahbub
 * Target PIC:  PIC32MX250F128B
 */

// graphics libraries
#include "config.h"
#include "tft_master.h"
#include "tft_gfx.h"
// need for rand function
#include <stdlib.h>

// threading library
// config.h sets 40 MHz
#define	SYS_FREQ 64000000 //40000000
#include "pt_cornell_TFT.h"

/* Demo code for interfacing TFT (ILI9340 controller) to PIC32
 * The library has been modified from a similar Adafruit library
 */
// Adafruit data:
/***************************************************
  This is an example sketch for the Adafruit 2.2" SPI display.
  This library works with the Adafruit 2.2" TFT Breakout w/SD card
  ----> http://www.adafruit.com/products/1480

  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

// string buffer
char buffer[60];

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_timer, pt_anim ;

// system 1 second interval tick
int sys_time_seconds ;
// code profile time
static int t1, t2;

// === Timer Thread =================================================
// update a 1 second tick counter
static PT_THREAD (protothread_timer(struct pt *pt))
{
    PT_BEGIN(pt);
     
      while(1) {
        // yield time 1 second
        PT_YIELD_TIME_msec(1000) ;
        sys_time_seconds++ ;
        
        // draw sys_time
        tft_fillRoundRect(0,300, 100, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 300);
        tft_setTextColor(ILI9340_YELLOW); tft_setTextSize(2);
        sprintf(buffer,"%d", sys_time_seconds);
        tft_writeString(buffer);
        
        // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
} // timer thread

// === Animation Thread =============================================
// update diffusing points
#define N 3000
static short x[N], y[N], vx, vy;
static PT_THREAD (protothread_anim(struct pt *pt))
{
    PT_BEGIN(pt);
      static int i;


      for (i=0; i<N; i++){
          x[i] = 120;
          y[i] = 160;
          tft_drawPixel(x[i], y[i], ILI9340_GREEN);
       }

      while(1) {
        // yield to let someting else run
        PT_YIELD(pt);

        t1 =  PT_GET_TIME();
        //t1 = ReadTimer2();
        // erase disk
        for (i=0; i<N; i++){
            tft_drawPixel(x[i], y[i], ILI9340_BLACK);

            // compute new position
            vx = 1 - 2*(rand() & 1);
            vy = 1 - 2*(rand() & 1);

            x[i] = x[i] + vx;
            if(x[i]>239)  x[i]=239;
            if(x[i]<1)  x[i]=1;

            y[i] = y[i] + vy;
            if(y[i]>319)  y[i]=319;
            if(y[i]<1)  y[i]=1;

            tft_drawPixel(x[i], y[i], ILI9340_WHITE);
        }
        t2 =  PT_GET_TIME();

        // draw time
        tft_fillRoundRect(0,10, 100, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 10);
        tft_setTextColor(ILI9340_YELLOW); tft_setTextSize(2);
        sprintf(buffer,"%d mSec", t2-t1);
        tft_writeString(buffer);

        // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
} // animation thread

// === Main  ======================================================
void main(void) {
 //SYSTEMConfigPerformance(PBCLK); already done in the PT setup
  
  ANSELA = 0; ANSELB = 0; CM1CON = 0; CM2CON = 0;

  // === config threads ==========
  // turns OFF UART support and debugger pin
  PT_setup();

  // code profile timer
  //OpenTimer2(T2_ON  | T2_SOURCE_INT | T5_PS_1_1 , 0xffff);

  // === setup system wide interrupts  ========
  INTEnableSystemMultiVectoredInt();

  // init the threads
  PT_INIT(&pt_timer);
  PT_INIT(&pt_anim);

  // init the display
  tft_init_hw();
  tft_begin();
  tft_fillScreen(ILI9340_BLACK);
  //240x320 vertical display
  tft_setRotation(0); // Use tft_setRotation(1) for 320x240

  // seed random gen
  srand(1);

  // round-robin scheduler for threads
  while (1){
      PT_SCHEDULE(protothread_timer(&pt_timer));
      PT_SCHEDULE(protothread_anim(&pt_anim));
      }
  } // main

// === end  ======================================================

