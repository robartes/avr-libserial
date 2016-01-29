#define F_CPU	8000000

#include <stdint.h>
#include <avr/io.h>
#include <util/delay.h>

#include "serial.h"

int main(void) 
{

	DDRB |= (1 << PB0);

	struct serial_config test_config = {
		SERIAL_SPEED_9600,	// speed
		PB1,				// TX pin
		&PORTB,				// TX port
		PB2,				// RX pin
		&PINB				// RX port
	};

	if (serial_initialise(&test_config) == SERIAL_OK) {

		// Test 1: do nothing to check timer frequency with scope
		while (0);

		// Test 2: write single character
		while (0) {
			serial_put_char(0x61);
			_delay_ms(100);
		}

		// Test 3: write more characters
		while (0) {
			serial_send_data("Bits of sand", 12);
		}

		// Test 4: two way communication
		//while (serial_data_pending()) {
		while (1) {
			if (serial_data_pending())
				serial_put_char(serial_get_char());	
				_delay_ms(100);
		}

	}

	return 0;
}
