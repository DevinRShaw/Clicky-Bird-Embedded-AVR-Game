#include <avr/io.h>
#include <avr/interrupt.h>
//#include <avr/signal.h>
#include <util/delay.h>
#include <stdlib.h>
#include "helper.h"
#ifndef PERIPH_H
#define PERIPH_H


//ADJUST THESE TO MATCH WHAT WE NEED FOR BUZZER MUSIC 

void TIMER1_init() {
	TCCR1A |= (1 << WGM11) | (1 << COM1A1); //COM1A1 sets it to channel A
	TCCR1B |= (1 << WGM12) | (1 << WGM13) | (1 << CS11); //CS11 sets the prescaler to be 8
	//WGM11, WGM12, WGM13 set timer to fast pwm mode
	ICR1 = 39999; //20ms pwm period
}

void TIMER0_init() {
	OCR0A = 255; //sets duty cycle to 50% since TOP is always 256

	TCCR0A |= (1 << COM0A1);// use Channel A
	TCCR0A |= (1 << WGM01) | (1 << WGM00);// set fast PWM Mode
	TCCR0B = (TCCR0B & 0xF8) | 0x02; //set prescaler to 8
	TCCR0B = (TCCR0B & 0xF8) | 0x03;//set prescaler to 64
	TCCR0B = (TCCR0B & 0xF8) | 0x04;//set prescaler to 256
	TCCR0B = (TCCR0B & 0xF8) | 0x05;//set prescaler to 1024
}

void yip() {
	OCR0A = 128; //sets duty cycle to 50% since TOP is always 256
	TCCR0B = (TCCR0B & 0xF8) | 0x04;//set prescaler to 64
}

void derp() {
	OCR0A = 128; //sets duty cycle to 50% since TOP is always 256
	TCCR0B = (TCCR0B & 0xF8) | 0x05;//set prescaler to 1024
}
void kill() {
	OCR0A = 255; //sets duty cycle to 50% since TOP is always 256
}


#endif /* PERIPH_H */