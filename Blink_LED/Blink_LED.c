#define F_CPU 1200000UL

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

int main(void){
	{
		asm volatile (					\
			"LDI R30,0xFF 		\n\t"	\
			"LDI R31,0x03 		\n\t"	\
			"LPM R30,Z    		\n\t"	\
			"CPI R30,0xFF 		\n\t"	\
			"BREQ OSCCAL_END_%= \n\t"	\
			"IN  R31,%[osccal]  \n\t"	\
			"OSCCAL_S_%=:		\n\t"	\
			"CP R31,R30 		\n\t"	\
			"BRSH OSCCAL_B_%=   \n\t"	\
			"INC R31 			\n\t"	\
			"OUT %[osccal],R31  \n\t"	\
			"RJMP OSCCAL_S_%=   \n\t"	\
			"OSCCAL_B_%=:		\n\t"	\
			"CP R30,R31 		\n\t"	\
			"BRSH OSCCAL_END_%= \n\t"	\
			"DEC R31 			\n\t"	\
			"OUT %[osccal],R31  \n\t"	\
			"RJMP OSCCAL_B_%=   \n\t"	\
			"OSCCAL_END_%=: 	\n\t"	\
		    ::[osccal] "I" (_SFR_IO_ADDR(OSCCAL)):"r30","r31" \
	  	);    
	}


	/*{
		unsigned char osccal_calibrated=pgm_read_byte(0x3FF);
		if (osccal_calibrated!=0xFF){
			while (osccal_calibrated > OSCCAL) OSCCAL++;
			while (osccal_calibrated < OSCCAL) OSCCAL--;
		}
	}*/

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
