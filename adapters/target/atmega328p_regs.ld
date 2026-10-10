/*
 * atmega328p_regs.ld - where each register declared in atmega328p_regs.h and
 * the per-peripheral headers beside it (USART0, Timer/Counter2, SPI) lives.
 *
 * The Makefile passes this file to the linker alongside the object files. Each
 * line gives a register name its data-space address from the ATmega328P
 * datasheet ("Register Summary"). Keep the files in step: a register needs
 * a declaration in one of the headers and an address here.
 *
 * A wrong address here links without any warning and makes the firmware write
 * to the wrong register, so check new entries against the datasheet.
 */

/* ---- I/O Ports: Input, Data Direction, and Output registers ---- */
PINB  = 0x23;
DDRB  = 0x24;
PORTB = 0x25;

PINC  = 0x26;
DDRC  = 0x27;
PORTC = 0x28;

PIND  = 0x29;
DDRD  = 0x2A;
PORTD = 0x2B;

/* ---- ADC (Analog to Digital Converter) ---- */
ADC    = 0x78;   /* 16-bit: ADCL at 0x78, ADCH at 0x79 */
ADCSRA = 0x7A;
ADMUX  = 0x7C;

/* ---- USART0 ---- */
UCSR0A = 0xC0;
UCSR0B = 0xC1;
UCSR0C = 0xC2;
UBRR0L = 0xC4;
UBRR0H = 0xC5;
UDR0   = 0xC6;

/* ---- SPI ---- */
SPCR = 0x4C;
SPSR = 0x4D;
SPDR = 0x4E;

/* ---- CPU ---- */
SREG = 0x5F;

/* ---- Timer/Counter2 ---- */
TIMSK2 = 0x70;
TCCR2A = 0xB0;
TCCR2B = 0xB1;
TCNT2  = 0xB2;
OCR2A  = 0xB3;
