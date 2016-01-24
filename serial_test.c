#define F_CPU	8000000

#include <stdint.h>
#include <avr/io.h>

#include "serial.h"

int main(void) 
{

	DDRB |= (1 << PB0);

	struct serial_config test_config = {
		SERIAL_SPEED_9600,	// speed
		PB1,				// TX pin
		&PORTB,				// TX port
		PB2,				// RX pin
		&PORTB				// RX port
	};

	if (serial_initialise(&test_config) == SERIAL_OK) {

		// Test 1: do nothing to check timer frequency with scope
		while (1);

		// Test 2: write single character
		serial_put_char(65);

		// Test 3: write more characters
		serial_send_data("Bits of sand", 12);

	}

	return 0;
}
