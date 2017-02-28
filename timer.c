/* 
 * File:   timer.c
 * Author: Nam Nguyen
 *
 * Created on February 25, 2017, 12:25 PM
 * 
 * pic18 has 4 timer modules
 * timer0 
 * - can be configured to be 8or16 bit timer/counter
 * - 8 bit mode set T0CON<6> (6th bit)
 * - has 3 registers:
 *  - T0CON (control register)
 *  - TMR0L (lower data)
 *  - TMR0H (higher data)
 * timer0 operates as timer or counter depending on clock source
 * timer - internal instruction cycle clock
 * counter - RA4/T0CLK pin (set T0CON<5> to get counter mode)
 * - to start counter/timer set T0CON<7>
 * 
 * prescaler (clear T0CON<3> to assign prescaler)
 * - can increase number of pulses per increment of counter/timer in pow of 2
 * - max 256, min 2
 * - prescaler value determined by T0CON<2:0> -> 111 is 1/256 and 000 is 1/2
 * 
 * TMR0H is buffered version of real high bit
 * writing to it stores the value in a buffer
 * only when TMR0L is set TMR0H is set simultaneously
 * 
 * TMR0 interrupt generated when TMR0 register overflows 
 * from 0xff t0 0x00 (8bit)
 * from 0xffff to 0x0000 (16bit)
 * 
 * interrup is set by setting INTCON<5>
 * overflow sets flag INTCON<2>
 *
 * the external oscillator in use ~10MHz
 * experimental measurement: 9.982464MHz +- 0.000500Hz
 * 
 * 9982464 - too slow
 * 9959472 - too slow
 * 9893064 - too slow
 * 9884827 - just right?
 * 9826656 - too fast
 */

#include <xc.h>
#include <stdio.h>
#include "timer.h"
#include "I2C.h"

long extFreq = 32000000; //in Hertz

void initT0(){
    //timer0 stuff
    TMR0IE = 1; //enable timer0 interrupt
    PEIE = 1; //turn off interrupt priorities
    ei(); //enable general interrupts
}

void startT0(float milliseconds){
    //timer counts up from the time set
    //so we need to subtract from maximum possible value (0xFFFF) the time that
    //we want to elapse, 4 clock pulses per instruction, prescaler of 128 
    //instructions per timer count. Dividing by 1000.0 to convert to seconds
    long time = 0xFFFF - ( milliseconds / 1000.0 ) * extFreq / 4.0 / 256.0;
    
    T0CON = 0; //clear before setting
    //default 16bit timer
    //T0CON |= 1<<3; //turn off prescaler
    T0CON |= 0b111; // prescaler = 2^(0b110 + 1)) = 128
    TMR0H = time>>8;
    TMR0L = time & 0xFF;
    T0CON |= 1<<7; //start timer
}

//times number of oscillations of crystal in 1 second
float testFrequency(){
    const char datetime[7] = {
        0x45, //45 Seconds 
        0x59, //59 Minutes
        0x23, //24 hour mode, set to 23:00
        0x07, //Saturday 
        0x31, //31st
        0x12, //December
        0x16  //2016
    };
    
    di();
    I2C_Master_Init(10000); //initialize master with 100KHz clock
    
    I2C_Master_Start();
        I2C_Master_Write(0b11010000); //rtc address + write
        I2C_Master_Write(0x00); //set pointer to seconds
        for(char i = 0; i < 7; i++)I2C_Master_Write(datetime[i]);
    I2C_Master_Stop();
    
    int time[7];
    int prev = 0;
    int timerOff = 1;
    int first = 1;
    ei();
    
    while(1){
        //Reset RTC memory pointer 
        I2C_Master_Start(); //Start condition
        I2C_Master_Write(0b11010000); //7 bit RTC address + Write
        I2C_Master_Write(0x00); //Set memory pointer to seconds
        I2C_Master_Stop(); //Stop condition

        //Read Current Time
        I2C_Master_Start();
        I2C_Master_Write(0b11010001); //7 bit RTC address + Read
        
        for(unsigned char i=0;i<0x06;i++){
            time[i] = I2C_Master_Read(1);
        }
        time[6] = I2C_Master_Read(0);       //Final Read without ack
        I2C_Master_Stop();
        
        if(time[0]^prev){ //if time changes (is different from prev value))
            if(first) //ignore the first difference in time[0] and prev
                first = 0;
            else
                if(timerOff){
                    printf("t1: %x ",time[0]);
                    initT0();
                    T0CON = 0; //clear before setting
                    T0CON |= 0b110; // prescaler = 2^(0b110 + 1)) = 128
                    TMR0H = 0;
                    TMR0L = 0;
                    T0CON |= 1<<7; //start timer
                    timerOff = 0;
                }
                else
                {
                    di();
                    printf("[%x %x]\n",TMR0L, TMR0H);
                    T0CON = 0; //stop timer
                    
                    //process results
                    // 4 clock pulses per instruction
                    // prescaler = 1/128 -> 128 instructions per count increment of clock
                    long long count = TMR0L + (TMR0H<<8);
                    return count*128*4 / 1000000.0; //convert to MHz
                }
            
        }
        
        prev = time[0];
    }
    return -1;
}