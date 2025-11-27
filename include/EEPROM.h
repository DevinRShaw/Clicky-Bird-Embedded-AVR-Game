#ifndef EEPROM_H
#define EEPROM_H
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#define EEPROM_SCORE_ADDR 0x3FF  // last byte of EEPROM


//directly taken from the datasheet 
void EEPROM_write_score(unsigned int uiAddress, unsigned char ucData){
    /* Wait for completion of previous write */
    while(EECR & (1<<EEPE));
    /* Set up address and Data Registers */
    EEAR = uiAddress;
    EEDR = ucData;
    /* Write logical one to EEMPE */
    EECR |= (1<<EEMPE);
    /* Start eeprom write by setting EEPE */
    EECR |= (1<<EEPE);
}

unsigned char EEPROM_read(unsigned int uiAddress){
    /* Wait for completion of previous write */
    while(EECR & (1<<EEPE))
    ;
    /* Set up address register */
    EEAR = uiAddress;
    /* Start eeprom read by writing EERE */
    EECR |= (1<<EERE);
    /* Return data from Data Register */
    return EEDR;
}
#endif 