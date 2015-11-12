
// graphics libraries
#include "config.h"
#include "tft_master.h"
#include "tft_gfx.h"
// need for rand function
#include <stdlib.h>
// serial stuff
#include <stdio.h>
// threading library
// config.h sets 40 MHz
#define	SYS_FREQ 64000000 //40000000
#include "pt_cornell_TFT.h"

// string buffer
char buffer[120];
static int rpm_buff[7];
int curr_rpm_cntr = 0;

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_timer, pt_rpm, pt_pid ;

// system 1 second interval tick
int sys_time_seconds ;
// code profile time
static int t1, t2;

static volatile int captureTime = 0; // Time that input capture captured
static volatile float RPM = 0; // Measured rotations per minute of fan
static volatile int bladeCounter = 0; // How many blades have passed since the last full rotation

// Setup for the output compare
static volatile int rising_edge_motor = 1;
static volatile int falling_edge_motor = 3;

// PID variables
static float desiredRPM = 1000;  // The RPM we want to set the motor to

static float error; // The difference between the desired RPM and the RPM measured 
static float prevError;  // The error of the previous measurement
static float derivativeError;  // Derivative of the error
static float integralError;  // Integral of the error

static float P = 5.0;  // Proportional gain term
static float I = 0.014;  // Integral gain term
static float D = 15.0;  // Derivative gain term

static float outputPID;  // The control signal calculated by the PID algorithm

// === thread structures ============================================
// semaphores for controlling two threads
// for guarding the UART and for allowing stread blink control
// thread control structs
// note that UART input and output are threads
static struct pt pt3, pt_input, pt_output, pt_DMA_output ;
// turn threads 1 and 2 on/off and set thread timing
int sys_time_seconds ;


// === Thread 3 ======================================================
// The serial interface
static char cmd[16]; 
static int value;

void IC1setup(){
    //Setup Input Capture 1
    
    //Set the Input Capture input to RPB13
    IC1R = 0x0003;
    //Turn IC1 On, Set it to 32 bits, set to user timer 2, set it to 1 capture per interrupt,
    //and set it to capture on every rising edge
    OpenCapture1(IC_ON | IC_FEDGE_RISE | IC_TIMER2_SRC | IC_INT_1CAPTURE | IC_EVERY_RISE_EDGE); 
    
    //Configure the input capture interrupt
    ConfigIntCapture1(IC_INT_ON | IC_INT_PRIOR_2);
    mIC1ClearIntFlag();
    //Enable the interrupt on capture event
    EnableIntIC1;
    
}


static float mult = 1.0;
static PT_THREAD (protothread3(struct pt *pt))
{
    PT_BEGIN(pt);
      while(1) {
          
            // send the prompt via DMA to serial
            sprintf(PT_send_buffer,"Enter Command:");
            // by spawning a print thread
            PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
 
          //spawn a thread to handle terminal input
            // the input thread waits for input
            // -- BUT does NOT block other threads
            // string is returned in "PT_term_buffer"
            PT_SPAWN(pt, &pt_input, PT_GetSerialBuffer(&pt_input) );
            // returns when the thead dies
            // in this case, when <enter> is pushed
            // now parse the string
             sscanf(PT_term_buffer, "%s %d", cmd, &value);

          
             int decPt = 2;
             int k;
             for(k = 1;k<16;k++){
                 if(cmd[k] == '.'){
                     decPt = k;
                     break;
                 }
                 if(cmd[k] == 0){
                     decPt = k;
                     break;
                 }
             }
             printf("%d\n\r", decPt);
             float num = 0;
             float mult = 1;
             
             for(k = decPt-1;k>0;k--){
                 num += ((cmd[k] - 48) * mult);
                 mult = mult*10;
                
             }
             printf("%f\n\r", num);
             
             
             mult = .1;
             
                for(k = decPt + 1;k<16;k++){
                    if(cmd[k] == 0){
                        break;
                    }else{
                        num += ((cmd[k]-48) * mult);
                        mult = mult*.1;
                    }
                }
            
             printf("%f\n\r", num);
             // scheduler control
             if (cmd[0] == 'p'){

                 float p = 0.0;

                 p = num;
                 P = p;
                 printf("%f\n\r", p);
                

             }
             
             if (cmd[0] == 'i'){

                    float i = 0.0;

                    i = num;
                     I = i;
                 printf("%f\n\r", i);
               

             }
             
               if (cmd[0] == 'd'){
                 //Skip for a space

                      float d = 0.0;

                      d = num;
                      D = d;
                 printf("%f\n\r", d);
                

             }
             
             if (cmd[0] == 's'){
                 //Skip for a space
                    float s = 0.0;

                    s = num;
                    desiredRPM = s;
                 printf("%f\n\r", s);
                

             }
             if (cmd[0] == 'z') printf("%d\t%d\n\r", PT_GET_TIME(), sys_time_seconds);
             
             int cntr;
             for(cntr = 0;cntr<16;cntr++){
                 cmd[cntr] = 0;
             }
            // never exit while
      } // END WHILE(1)
  PT_END(pt);
} // thread 3

// === Timer Thread =================================================
// update a 1 second tick counter
static PT_THREAD (protothread_timer(struct pt *pt))
{
    PT_BEGIN(pt);
     
      while(1) {
        // yield time 1 second
        PT_YIELD_TIME_msec(1000);
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

static PT_THREAD (protothread_pid(struct pt *pt))
{
    PT_BEGIN(pt);
     
      while(1) {
		  
        //run 100 times a second
        PT_YIELD_TIME_msec(10);
		// Calculate error from newest RPM measurement
        error = desiredRPM - RPM;
        
		// Calculate the integral error
		// If the measured RPM hasn't crossed the desired RPM threashold just add the error
        if((error > 0) && (prevError > 0)){
                integralError = integralError + error;
        }
        else{
            if((error <= 0) && (prevError <= 0)){
                   integralError = integralError + error; 
                
            }
            else{ 
				// If the measured error crossed the desired RPM threashold set the integral error to 90% to prevent a sudden drop in RPM
                integralError = .9 * integralError;
               
            }
        }
        // Calculate derivative term
        derivativeError = error - prevError;
		
		// Set the previous error for the next time through the thread
        prevError = error;

        
        
        // Calculate the control signal
        outputPID = 100.0 * (P*error + I*integralError + D*derivativeError);
		// If the output of the PID is less than zero stop driving the fan to slow it down
        if((int)outputPID < 0){
            OC1RS = 0;
        }else{
			// If the output of the PID is above 64000 (our maximum speed) drive the fan at full speed with a full duty cycle
            if((int)outputPID > 0){
                if((int)outputPID > 64000){
                    OC1RS = 64000;
                }else{
					// If inbetween zero and the max speed the duty cycle is set as the output of the PID
                    OC1RS = outputPID;
                }
            }
        }
    }
        
  PT_END(pt);
} // timer thread

// Displays information about the PID and fan
static PT_THREAD (protothread_rpm(struct pt *pt))
{
    PT_BEGIN(pt);
     
      while(1) {
        // yield time 1 second
        PT_YIELD_TIME_msec(1000);
        
		// Display the RPM, Desired RPM, Error, P gain, I gain, and D gain on the TFT
        tft_setCursor(0, 20);tft_setTextColor(ILI9340_MAGENTA); 
        tft_setTextSize(2);
        tft_writeString("RPM: ");
        
        tft_fillRoundRect(0,40, 200, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 40);
        tft_setTextColor(ILI9340_CYAN); 
        tft_setTextSize(2);
        sprintf(buffer,"%f", RPM);
        tft_writeString(buffer);
        
        tft_setCursor(0, 60);tft_setTextColor(ILI9340_MAGENTA); 
        tft_setTextSize(2);
        tft_writeString("Desired Speed: ");
        
        tft_fillRoundRect(0,80, 200, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 80);
        tft_setTextColor(ILI9340_CYAN); 
        tft_setTextSize(2);
        sprintf(buffer,"%f", desiredRPM);
        tft_writeString(buffer);
        
         tft_setCursor(0, 100);tft_setTextColor(ILI9340_MAGENTA); 
        tft_setTextSize(2);
        tft_writeString("Error: ");
        
        tft_fillRoundRect(0,120, 200, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 120);
        tft_setTextColor(ILI9340_CYAN); 
        tft_setTextSize(2);
        sprintf(buffer,"%f", error);
        tft_writeString(buffer);
        
         tft_setCursor(0, 140);tft_setTextColor(ILI9340_MAGENTA); 
        tft_setTextSize(2);
        tft_writeString("P: ");
        
        tft_fillRoundRect(0,160, 200, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 160);
        tft_setTextColor(ILI9340_CYAN); 
        tft_setTextSize(2);
        sprintf(buffer,"%f", P);
        tft_writeString(buffer);
        
          tft_setCursor(0, 180);tft_setTextColor(ILI9340_MAGENTA); 
        tft_setTextSize(2);
        tft_writeString("I: ");
        
        tft_fillRoundRect(0,200, 200, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 200);
        tft_setTextColor(ILI9340_CYAN); 
        tft_setTextSize(2);
        sprintf(buffer,"%f", I);
        tft_writeString(buffer);
        
          tft_setCursor(0, 220);tft_setTextColor(ILI9340_MAGENTA); 
        tft_setTextSize(2);
        tft_writeString("D: ");
        
        tft_fillRoundRect(0,240, 200, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 240);
        tft_setTextColor(ILI9340_CYAN); 
        tft_setTextSize(2);
        sprintf(buffer,"%f", D);
        tft_writeString(buffer);
        
        
        
      } // END WHILE(1)
  PT_END(pt);
} // timer thread

//Input Capture Event Interrupt
void __ISR(_INPUT_CAPTURE_1_VECTOR, ipl2) IC1Handler(void)
{
	// Read the time that a blade passed to get it out of the buffer
    captureTime = mIC1ReadCapture();
	// Count how many blades have passed 
    bladeCounter++;
	// If 7 blades have passed the fan has completed a full rotation
    if(bladeCounter == 7){
        // reset the timer to measure the next rotation
        WriteTimer2(0x0000);
		// calculate the RPM from the captured time
        RPM = 60.0*(((float)(250000))/((float)(captureTime)));
        rpm_buff[curr_rpm_cntr] = RPM;
        curr_rpm_cntr = ((curr_rpm_cntr+1)%7);
        
		// Reset blade counter for next rotation
        bladeCounter = 0;
        
		// Set a duty cycle based on the RPM to output to an oscilliscope
        OC2RS = (int)(((float)RPM / 3000.0) * 64000);
    }
   
    mIC1ClearIntFlag();

}



// === Main  ======================================================
void main(void) {

  // === config threads ==========
  // turns OFF UART support and debugger pin
  PT_setup();


  // === setup system wide interrupts  ========
  INTEnableSystemMultiVectoredInt();

  // Set up timer 2 for input capture at 250kHz
  OpenTimer2(T2_ON  | T2_SOURCE_INT | T2_PS_1_256, 65535);
  // set up the timer interrupt with a priority of 2
  ConfigIntTimer2(T2_INT_OFF | T2_INT_PRIOR_2);
  
  
  //period of 64000 should make the timer overflow frequency 1000 Hz, the desired PWM frequency
  OpenTimer3(T3_ON | T3_SOURCE_INT | T3_PS_1_1, 64000);
  ConfigIntTimer2(T3_INT_OFF | T3_INT_PRIOR_2);
  
  //OC STUFF
  // set pulse to go high at 1/4 of the timer period and drop again at 1/2 the timer period
  // ONLY TIMER 2 AND 3 CAN BE USED WITH OC. CAN CHANGE THE PROTOTHREADS TIMER COUNTER
  // TO LET US USE 2 HERE AND TIMER 1 FOR PROTOTHREADS AND 45 FOR RPM
  OpenOC1(OC_ON | OC_TIMER3_SRC | OC_PWM_FAULT_PIN_DISABLE , falling_edge_motor, rising_edge_motor);
  // OC1 is PPS group 1, map to RPA0 (pin 2)
  PPSOutput(1, RPA0, OC1);
  
  OpenOC2(OC_ON | OC_TIMER3_SRC | OC_PWM_FAULT_PIN_DISABLE, 64000, 0);
  // OC1 is PPS group 1, map to RPA0 (pin 2)
  PPSOutput(2, RPB5, OC2);
  
  IC1setup();
  // init the threads
  PT_INIT(&pt_rpm);
  PT_INIT(&pt_timer);
  PT_INIT(&pt_pid);
  PT_INIT(&pt3);
  
  // init the display
  tft_init_hw();
  tft_begin();
  tft_fillScreen(ILI9340_BLACK);
  //240x320 vertical display
  tft_setRotation(0); // Use tft_setRotation(1) for 320x240

  // round-robin scheduler for threads
  while (1){
      PT_SCHEDULE(protothread_rpm(&pt_rpm));
      PT_SCHEDULE(protothread_timer(&pt_timer));
      PT_SCHEDULE(protothread_pid(&pt_pid));
      PT_SCHEDULE(protothread3(&pt3));
      }
  } // main

// === end  ======================================================

