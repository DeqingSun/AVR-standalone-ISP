#define F_CPU 1200000UL

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

int main(void){
	OSCCAL=pgm_read_byte(0x3FF);

	DDRB|=(1<<PB1);
	DDRB|=(1<<PB4);
	TCCR0A=(0b01<<COM0B0)|(0b10<<WGM00);	//CTC mode
	TCCR0B=(0b010<<CS00);
	OCR0A=149;

	while(1){
		_delay_ms(500);
		PORTB^=(1<<PB4);
	}
	return 1;
}
