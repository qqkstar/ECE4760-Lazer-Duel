/* 
 * File:   pt_cornell.h
 * Author: brl4
 *
 * Created on November 18, 2014, 9:12 AM
 */

/*
 * Copyright (c) 2004-2005, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 * $Id: pt.h,v 1.7 2006/10/02 07:52:56 adam Exp $
 */

/**
 * \addtogroup pt
 * @{
 */

/**
 * \file
 * Protothreads implementation.
 * \author
 * Adam Dunkels <adam@sics.se>
 *
 */

#ifndef __PT_H__
#define __PT_H__

#include "lc.h"

struct pt {
  lc_t lc;
  int pri;
};

#define PT_WAITING 0
#define PT_YIELDED 1
#define PT_EXITED  2
#define PT_ENDED   3

/**
 * \name Initialization
 * @{
 */

/**
 * Initialize a protothread.
 *
 * Initializes a protothread. Initialization must be done prior to
 * starting to execute the protothread.
 *
 * \param pt A pointer to the protothread control structure.
 *
 * \sa PT_SPAWN()
 *
 * \hideinitializer
 */
#define PT_INIT(pt)   LC_INIT((pt)->lc)

/** @} */

/**
 * \name Declaration and definition
 * @{
 */

/**
 * Declaration of a protothread.
 *
 * This macro is used to declare a protothread. All protothreads must
 * be declared with this macro.
 *
 * \param name_args The name and arguments of the C function
 * implementing the protothread.
 *
 * \hideinitializer
 */
#define PT_THREAD(name_args) char name_args

/**
 * Declare the start of a protothread inside the C function
 * implementing the protothread.
 *
 * This macro is used to declare the starting point of a
 * protothread. It should be placed at the start of the function in
 * which the protothread runs. All C statements above the PT_BEGIN()
 * invokation will be executed each time the protothread is scheduled.
 *
 * \param pt A pointer to the protothread control structure.
 *
 * \hideinitializer
 */
#define PT_BEGIN(pt) { char PT_YIELD_FLAG = 1; LC_RESUME((pt)->lc)

/**
 * Declare the end of a protothread.
 *
 * This macro is used for declaring that a protothread ends. It must
 * always be used together with a matching PT_BEGIN() macro.
 *
 * \param pt A pointer to the protothread control structure.
 *
 * \hideinitializer
 */
#define PT_END(pt) LC_END((pt)->lc); PT_YIELD_FLAG = 0; \
                   PT_INIT(pt); return PT_ENDED; }

/** @} */

/**
 * \name Blocked wait
 * @{
 */

/**
 * Block and wait until condition is true.
 *
 * This macro blocks the protothread until the specified condition is
 * true.
 *
 * \param pt A pointer to the protothread control structure.
 * \param condition The condition.
 *
 * \hideinitializer
 */
#define PT_WAIT_UNTIL(pt, condition)	        \
  do {						\
    LC_SET((pt)->lc);				\
    if(!(condition)) {				\
      return PT_WAITING;			\
    }						\
  } while(0)

/**
 * Block and wait while condition is true.
 *
 * This function blocks and waits while condition is true. See
 * PT_WAIT_UNTIL().
 *
 * \param pt A pointer to the protothread control structure.
 * \param cond The condition.
 *
 * \hideinitializer
 */
#define PT_WAIT_WHILE(pt, cond)  PT_WAIT_UNTIL((pt), !(cond))

/** @} */

/**
 * \name Hierarchical protothreads
 * @{
 */

/**
 * Block and wait until a child protothread completes.
 *
 * This macro schedules a child protothread. The current protothread
 * will block until the child protothread completes.
 *
 * \note The child protothread must be manually initialized with the
 * PT_INIT() function before this function is used.
 *
 * \param pt A pointer to the protothread control structure.
 * \param thread The child protothread with arguments
 *
 * \sa PT_SPAWN()
 *
 * \hideinitializer
 */
#define PT_WAIT_THREAD(pt, thread) PT_WAIT_WHILE((pt), PT_SCHEDULE(thread))

/**
 * Spawn a child protothread and wait until it exits.
 *
 * This macro spawns a child protothread and waits until it exits. The
 * macro can only be used within a protothread.
 *
 * \param pt A pointer to the protothread control structure.
 * \param child A pointer to the child protothread's control structure.
 * \param thread The child protothread with arguments
 *
 * \hideinitializer
 */
#define PT_SPAWN(pt, child, thread)		\
  do {						\
    PT_INIT((child));				\
    PT_WAIT_THREAD((pt), (thread));		\
  } while(0)

/** @} */

/**
 * \name Exiting and restarting
 * @{
 */

/**
 * Restart the protothread.
 *
 * This macro will block and cause the running protothread to restart
 * its execution at the place of the PT_BEGIN() call.
 *
 * \param pt A pointer to the protothread control structure.
 *
 * \hideinitializer
 */
#define PT_RESTART(pt)				\
  do {						\
    PT_INIT(pt);				\
    return PT_WAITING;			\
  } while(0)

/**
 * Exit the protothread.
 *
 * This macro causes the protothread to exit. If the protothread was
 * spawned by another protothread, the parent protothread will become
 * unblocked and can continue to run.
 *
 * \param pt A pointer to the protothread control structure.
 *
 * \hideinitializer
 */
#define PT_EXIT(pt)				\
  do {						\
    PT_INIT(pt);				\
    return PT_EXITED;			\
  } while(0)

/** @} */

/**
 * \name Calling a protothread
 * @{
 */

/**
 * Schedule a protothread.
 *
 * This function shedules a protothread. The return value of the
 * function is non-zero if the protothread is running or zero if the
 * protothread has exited.
 *
 * \param f The call to the C function implementing the protothread to
 * be scheduled
 *
 * \hideinitializer
 */
#define PT_SCHEDULE(f) ((f) < PT_EXITED)
//#define PT_SCHEDULE(f) ((f))

/** @} */

/**
 * \name Yielding from a protothread
 * @{
 */

/**
 * Yield from the current protothread.
 *
 * This function will yield the protothread, thereby allowing other
 * processing to take place in the system.
 *
 * \param pt A pointer to the protothread control structure.
 *
 * \hideinitializer
 */
#define PT_YIELD(pt)				\
  do {						\
    PT_YIELD_FLAG = 0;				\
    LC_SET((pt)->lc);				\
    if(PT_YIELD_FLAG == 0) {			\
      return PT_YIELDED;			\
    }						\
  } while(0)

/**
 * \brief      Yield from the protothread until a condition occurs.
 * \param pt   A pointer to the protothread control structure.
 * \param cond The condition.
 *
 *             This function will yield the protothread, until the
 *             specified condition evaluates to true.
 *
 *
 * \hideinitializer
 */
#define PT_YIELD_UNTIL(pt, cond)		\
  do {						\
    PT_YIELD_FLAG = 0;				\
    LC_SET((pt)->lc);				\
    if((PT_YIELD_FLAG == 0) || !(cond)) {	\
      return PT_YIELDED;                        \
    }						\
  } while(0)

/** @} */

#endif /* __PT_H__ */

#ifndef __PT_SEM_H__
#define __PT_SEM_H__

#include "pt.h"

struct pt_sem {
  unsigned int count;
};

/**
 * Initialize a semaphore
 *
 * This macro initializes a semaphore with a value for the
 * counter. Internally, the semaphores use an "unsigned int" to
 * represent the counter, and therefore the "count" argument should be
 * within range of an unsigned int.
 *
 * \param s (struct pt_sem *) A pointer to the pt_sem struct
 * representing the semaphore
 *
 * \param c (unsigned int) The initial count of the semaphore.
 * \hideinitializer
 */
#define PT_SEM_INIT(s, c) (s)->count = c

/**
 * Wait for a semaphore
 *
 * This macro carries out the "wait" operation on the semaphore. The
 * wait operation causes the protothread to block while the counter is
 * zero. When the counter reaches a value larger than zero, the
 * protothread will continue.
 *
 * \param pt (struct pt *) A pointer to the protothread (struct pt) in
 * which the operation is executed.
 *
 * \param s (struct pt_sem *) A pointer to the pt_sem struct
 * representing the semaphore
 *
 * \hideinitializer
 */
#define PT_SEM_WAIT(pt, s)	\
  do {						\
    PT_WAIT_UNTIL(pt, (s)->count > 0);		\
    --(s)->count;				\
  } while(0)

/**
 * Signal a semaphore
 *
 * This macro carries out the "signal" operation on the semaphore. The
 * signal operation increments the counter inside the semaphore, which
 * eventually will cause waiting protothreads to continue executing.
 *
 * \param pt (struct pt *) A pointer to the protothread (struct pt) in
 * which the operation is executed.
 *
 * \param s (struct pt_sem *) A pointer to the pt_sem struct
 * representing the semaphore
 *
 * \hideinitializer
 */
#define PT_SEM_SIGNAL(pt, s) ++(s)->count

#endif /* __PT_SEM_H__ */

//=====================================================================
//=== BRL4 additions for PIC 32 =======================================
//=====================================================================

// macro to time a thread execution interveal in millisec
// max time 4000 sec
#include <limits.h>
#define PT_YIELD_TIME_msec(delay_time)  \
    do { static unsigned int time_thread ;\
    time_thread = time_tick_millsec + (unsigned int)delay_time ; \
    PT_YIELD_UNTIL(pt, (time_tick_millsec >= time_thread)); \
    } while(0);

// macro to time a thread execution interveal
// parameter is in MICROSEC
// Max time is 4000 sec
//#define PT_YIELD_TIME_usec(delay_time)  \
 //   do { static unsigned int time_thread, delta=0, time45; time45=ReadTimer45();\
 ///   time_thread = time45 + (unsigned int)delay_time ; \
 //   if (time_thread < time45) {delta = UINT_MAX - time45; } \
 //    PT_YIELD_UNTIL(pt, (delta==0 && ReadTimer45()<time45) || (ReadTimer45()+delta >= time_thread+delta)); \
 //   } while(0);
// PT_YIELD_UNTIL(pt, ReadTimer45()+delta >= time_thread+delta);

//#define PT_YIELD_TIME_usec(delay_time) \
//    do { static signed int time_thread ; \
//      time_thread = (signed int)delay_time + (signed int)ReadTimer45() ; \
 //     PT_YIELD_UNTIL(pt, (signed int)ReadTimer45() >= time_thread); \
 //   } while(0);

// macro to return system time
#define PT_GET_TIME() (time_tick_millsec)

// init rate sehcduler
//#define PT_INIT(pt, priority)   LC_INIT((pt)->lc ; (pt)->pri = priority)
//PT_PRIORITY_INIT
#define PT_RATE_INIT() int pt_pri_count = 0;
// maitain proority frame count
//PT_PRIORITY_LOOP maitains a counter used to control execution
#define PT_RATE_LOOP() pt_pri_count = (pt_pri_count+1) & 0xf ;
// schecedule priority thread
//PT_PRIORITY_SCHEDULE
// 5 levels
// rate 0 is highest -- every time thru loop
// priority 1 -- every 2 times thru loop
// priority 2 -- every 4 times thru loop
//  3 is  -- every 8 times thru loop
#define PT_RATE_SCHEDULE(f,rate) \
    if((rate==0) | \
    (rate==1 && ((pt_pri_count & 0b1)==0) ) | \
    (rate==2 && ((pt_pri_count & 0b11)==0) ) | \
    (rate==3 && ((pt_pri_count & 0b111)==0)) | \
    (rate==4 && ((pt_pri_count & 0b1111)==0))) \
        PT_SCHEDULE(f);

// macro to use 4 bit DAC as debugger output
// level range 0-15; duration in microseconds
// -- with zero meaning HOLD it on forever
//while((signed int)ReadTimer45() <= time_hold){};
// time_hold = duration + ReadTimer45() ;
#define PT_DEBUG_VALUE(level, duration) \
do { static int i ; \
    CVRCON = CVRCON_setup | (level & 0xf); \
    if (duration>0){                   \
        for (i=0; i<duration*7; i++){};\
        CVRCON = CVRCON_setup; \
    } \
} while(0);

// macros to manipulate a semaphore without blocking
#define PT_SEM_SET(s) (s)->count=1
#define PT_SEM_CLEAR(s) (s)->count=0
#define PT_SEM_READ(s) (s)->count
#define PT_SEM_ACCEPT(s) \
  s->count; \
  if (s->count) s->count-- ; \

//====================================================================
//=== serial setup ===================================================
// UART parameters
#define BAUDRATE 9600 // must match PC end
#define PB_DIVISOR (1 << OSCCONbits.PBDIV) // read the peripheral bus divider, FPBDIV
#define PB_FREQ SYS_FREQ/PB_DIVISOR // periperhal bus frequency
#define clrscr() printf( "\x1b[2J")
#define home()   printf( "\x1b[H")
#define pcr()    printf( '\r')
#define crlf     putchar(0x0a); putchar(0x0d);
#define backspace 0x7f // make sure your backspace matches this!
#define max_chars 32 // for input/output buffer
//====================================================================
// build a string from the UART2 /////////////
//////////////////////////////////////////////
char PT_term_buffer[max_chars];
int num_char;
int PT_GetSerialBuffer(struct pt *pt)
{
    static char character;
    // mark the beginnning of the input thread
    PT_BEGIN(pt);

    num_char = 0;
    //memset(term_buffer, 0, max_chars);

    while(num_char < max_chars)
    {
        // get the character
        // yield until there is a valid character so that other
        // threads can execute
        PT_YIELD_UNTIL(pt, UARTReceivedDataIsAvailable(UART2));
       // while(!UARTReceivedDataIsAvailable(UART2)){};
        character = UARTGetDataByte(UART2);
        PT_YIELD_UNTIL(pt, UARTTransmitterIsReady(UART2));
        UARTSendDataByte(UART2, character);

        // unomment to check backspace character!!!
        //printf("--%x--",character );

        // end line
        if(character == '\r'){
            PT_term_buffer[num_char] = 0; // zero terminate the string
            //crlf; // send a new line
            PT_YIELD_UNTIL(pt, UARTTransmitterIsReady(UART2));
            UARTSendDataByte(UART2, '\n');
            break;
        }
        // backspace
        else if (character == backspace){
            PT_YIELD_UNTIL(pt, UARTTransmitterIsReady(UART2));
            UARTSendDataByte(UART2, ' ');
            PT_YIELD_UNTIL(pt, UARTTransmitterIsReady(UART2));
            UARTSendDataByte(UART2, backspace);
            num_char--;
            // check for buffer underflow
            if (num_char<0) {num_char = 0 ;}
        }
        else  {PT_term_buffer[num_char++] = character ;}
         //if (character == backspace)

    } //end while(num_char < max_size)

    // kill this input thread, to allow spawning thread to execute
    PT_EXIT(pt);
    // and indicate the end of the thread
    PT_END(pt);
}

//====================================================================
// === send a string to the UART2 ====================================
char PT_send_buffer[max_chars];
int num_send_chars ;
int PutSerialBuffer(struct pt *pt)
{
    PT_BEGIN(pt);
    num_send_chars = 0;
    while (PT_send_buffer[num_send_chars] != 0){
        PT_YIELD_UNTIL(pt, UARTTransmitterIsReady(UART2));
        UARTSendDataByte(UART2, PT_send_buffer[num_send_chars]);
        num_send_chars++;
    }
    // kill this output thread, to allow spawning thread to execute
    PT_EXIT(pt);
    // and indicate the end of the thread
    PT_END(pt);
}

//====================================================================
// === DMA send string to the UART2 ==================================
int PT_DMA_PutSerialBuffer(struct pt *pt)
{
    PT_BEGIN(pt);
    //mPORTBSetBits(BIT_0);
    // check for null string
    if (PT_send_buffer[0]==0)PT_EXIT(pt);
    // sent the first character
    PT_YIELD_UNTIL(pt, UARTTransmitterIsReady(UART2));
    UARTSendDataByte(UART2, PT_send_buffer[0]);
    //DmaChnStartTxfer(DMA_CHANNEL1, DMA_WAIT_NOT, 0);
    // start the DMA
    DmaChnEnable(DMA_CHANNEL1);
    // wait for DMA done
    //mPORTBClearBits(BIT_0);
    PT_YIELD_UNTIL(pt, DmaChnGetEvFlags(DMA_CHANNEL1) & DMA_EV_BLOCK_DONE);

    // kill this output thread, to allow spawning thread to execute
    PT_EXIT(pt);
    // and indicate the end of the thread
    PT_END(pt);
}

//======================================================================
// === setup uart and DMA and timer and vREF
// debugging putput global
int CVRCON_setup ;
unsigned int time_tick_millsec ;

// Timer 2 interrupt handler ///////
// ipl2 means "interrupt priority level 2"
void __ISR(_TIMER_1_VECTOR, ipl2) Timer1Handler(void)
{
    // clear the interrupt flag
    mT1ClearIntFlag();
    //count millseconds
    time_tick_millsec++ ;
}

void PT_setup (void)
{

  // ===Set up timer5 ======================
  // timer 5: on,  interrupts, internal clock, prescalar 32,
  // ticks once/microsec!
  // set up to count millsec 40 MHz/32 = 1.25 MHz or 0.8 microsec per tick so
  // 1250 x 0.8 = 1 mSec
  OpenTimer1(T1_ON  | T1_SOURCE_INT | T1_PS_1_64 , 1000); //1250); 1999 => 64 MHz
  // set up the timer interrupt with a priority of 2
  ConfigIntTimer1(T1_INT_ON | T1_INT_PRIOR_2);
  mT1ClearIntFlag(); // and clear the interrupt flag
  // zero the system time tick
  time_tick_millsec = 0;


}