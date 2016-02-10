#define F_CPU	8000000

#include <stdint.h>
#include <avr/io.h>
#include <util/delay.h>

#include "serial.h"

int main(void) 
{

	DDRB |= (1 << PB0 | 1 << PB4);

	if (serial_initialise() == SERIAL_OK) {

		// Test 1: canary test
		while (0) {
		}

		// Test 2: write single character
		while (0) {
			serial_put_char(0x55);
			_delay_ms(100);
		}

		// Test 3: write more characters
		serial_enable_receive();
		while (0) {
			serial_send_data("Bits of sand", 12);
			_delay_ms(100);
		}

		// Test 4: two way communication
		//while (serial_data_pending()) {
		serial_enable_receive();
		while (1) {
			if (serial_data_pending()) {

				serial_put_char(serial_get_char());	
				_delay_ms(100);
			}
		}

	}

	return 0;
}
