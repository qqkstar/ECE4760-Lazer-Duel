/*
 * File:   config.h
 * Author: Syed Tahmid Mahbub
 *
 * Created on October 10, 2014
 */

#ifndef CONFIG_H
#define	CONFIG_H

// serial stuff
#include <stdio.h>

/*
#pragma config FNOSC = FRCPLL, POSCMOD = OFF
#pragma config FPLLIDIV = DIV_2, FPLLMUL = MUL_20
#pragma config FPBDIV = DIV_1, FPLLODIV = DIV_2
#pragma config FWDTEN = OFF, JTAGEN = OFF, FSOSCEN = OFF
*/
// 64 MHz!
#pragma config FNOSC = FRCPLL, POSCMOD = OFF
#pragma config FPLLIDIV = DIV_2, FPLLMUL = MUL_16 //64 MHz 
#pragma config FPBDIV = DIV_1, FPLLODIV = DIV_1
#pragma config FWDTEN = OFF, JTAGEN = OFF, FSOSCEN = OFF

#endif	/* CONFIG_H */

