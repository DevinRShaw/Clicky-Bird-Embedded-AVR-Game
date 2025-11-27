#ifndef SPIAVR_H
#define SPIAVR_H
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

//B5 should always be SCK(spi clock) and B3 should always be MOSI. If you are using an
//SPI peripheral that sends data back to the arduino, you will need to use B4 as the MISO pin.
//The SS pin can be any digital pin on the arduino. Right before sending an 8 bit value with
//the SPI_SEND() funtion, you will need to set your SS pin to low. If you have multiple SPI
//devices, they will share the SCK, MOSI and MISO pins but should have different SS pins.
//To send a value to a specific device, set it's SS pin to low and all other SS pins to high.

// Outputs, pin definitions BIT MASKS (FIXED!)
#define PIN_SCK      (1 << PORTB5)  // Pin 13 - SCK
#define PIN_MOSI     (1 << PORTB3)  // Pin 11 - MOSI
#define PIN_SS       (1 << PORTB2)  // Pin 10 - CS
#define A0           (1 << PORTB1)  // Pin 9 - DC
#define RESET_PIN    (1 << PORTB0)  // Pin 8  - RESET

void SPI_INIT(){
    DDRB |= PIN_SCK | PIN_MOSI | PIN_SS | A0 | RESET_PIN;  // Set as outputs
    PORTB|= PIN_SS;  // CS starts HIGH (deselected)
    SPCR |= (1 << SPE) | (1 << MSTR);  // Enable SPI, Master mode
}

void SPI_SEND(char data){
    SPDR = data;  // Set data to transmit
    while (!(SPSR & (1 << SPIF)));  // Wait until done transmitting
}

//PIN BASED RESET
void HardwareReset(){
    PORTB |= RESET_PIN;   // Pull RESET HIGH
    _delay_ms(500);
    PORTB &= ~RESET_PIN;  // Pull RESET LOW
    _delay_ms(500);
    PORTB |= RESET_PIN;   // Pull RESET HIGH
    _delay_ms(500);
}

void SendCommand(uint8_t command){
    PORTB &= ~PIN_SS;  // CS LOW - select display
    PORTB &= ~A0;      // DC LOW - command mode
    SPI_SEND(command);
    PORTB |= PIN_SS;   // CS HIGH - deselect display
}

// Command hex codes
#define SWRESET 0x01
#define SLPOUT  0x11
#define COLMOD  0x3A
#define DISPON  0x29 
#define MADCTL  0x36 
#define INVERT  0x21
#define REVERT  0x20

void ST7735_init(){
    HardwareReset();
    SendCommand(SWRESET);
    _delay_ms(500);
    SendCommand(SLPOUT);
    _delay_ms(500);
    SendCommand(COLMOD);
    PORTB &= ~PIN_SS;  // CS LOW - select display
    PORTB |= A0;       // DC HIGH - data mode
    SPI_SEND(0x05);  // 16-bit color mode (RGB565)
    PORTB |= PIN_SS;   // CS HIGH - deselect display
    _delay_ms(500);
    SendCommand(DISPON);
    _delay_ms(500);
    SendCommand(MADCTL); 
    PORTB &= ~PIN_SS;  // CS LOW - select display
    PORTB |= A0;       // DC HIGH - data mode
    SPI_SEND(0x80);  //1000 0000
    PORTB |= PIN_SS;   // CS HIGH - deselect display
}


#define CASET   0x2A
    #define XS  0x00
    #define XE  0x83
#define RASET   0x2B
    #define YS  0x00
    #define YE  0x83
#define RAMWR   0x2C

void SetWriteWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1){
    SendCommand(CASET);
        PORTB &= ~PIN_SS;  // CS LOW - select display
        PORTB |= A0;       // DC HIGH - data mode
        SPI_SEND(0x00);SPI_SEND(x0);           
        SPI_SEND(0x00);SPI_SEND(x1);           
        PORTB |= PIN_SS;   // CS HIGH - deselect display
    
    SendCommand(RASET);
        PORTB &= ~PIN_SS;  // CS LOW - select display
        PORTB |= A0;       // DC HIGH - data mode
        SPI_SEND(0x00);SPI_SEND(y0);           
        SPI_SEND(0x00);SPI_SEND(y1);           
        PORTB |= PIN_SS;   // CS HIGH - deselect display
    
    SendCommand(RAMWR);
}

void FillWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t color){

    uint32_t total = (x1 - x0 + 1) * (y1 - y0 + 1);  // total pixels
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;

    PORTB &= ~PIN_SS;   // CS LOW (start pixel stream)
    PORTB |= A0;        // DC HIGH (data mode)

    for(uint32_t i = 0; i < total; i++){
        SPI_SEND(hi);
        SPI_SEND(lo);
    }

    PORTB |= PIN_SS;    // CS HIGH (end pixel stream)
}



#endif /* SPIAVR_H */