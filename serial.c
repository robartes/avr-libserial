/************************************************************************
 * libserial
 *
 * AVR software serial library for ATTinyx5
 ************************************************************************/

#include <avr/io.h>
#include <stdint.h>
#include "serial.h"

#define NUM_SPEED  5 

/************************************************************************
 * state variables (global)
 ************************************************************************/

serial_state_t connection_state = SERIAL_NOT_INITIALISED;

/************************************************************************
 * Private functions
 ************************************************************************/


/************************************************************************
 * setup_io: what it says on the tin
 *
 * Parameters:
 *		volatile uint8_t *port	Memory address of the I/O port
 *		uint8_t pin				Port pin
 *		serial_direction_t dir  TX or RX port?
 *
 * This library is written for ATTinyx5 which only has PORTB, but the
 * vestiges of multiple port support are in here. 
 * For real multiple port support, some additional sanity checking and 
 * a DDRx vs PORTx lookup table would need to be implemented.
 ************************************************************************/

typedef enum {
	SERIAL_DIR_TX,
	SERIAL_DIR_RX,
} serial_direction_t;

static return_code_t setup_io(
				volatile uint8_t *port,
				uint8_t pin,
				serial_direction_t dir
				)
{

	// Sanity checks
	if (port != &PORTB) 
		return SERIAL_ERROR;

	if (pin > 5)
		return SERIAL_ERROR;

	switch (dir) {
		
		case SERIAL_DIR_TX:

			DDRB |= (1 << pin);
			PORTB |= (1 << pin);  // Set high (idle)
			break;

		case SERIAL_DIR_RX:
		
			DDRB &= ~(1 << pin);
			PORTB &= ~(1 << pin);  // No pullup
			break;

	}

    return SERIAL_OK;

}

/************************************************************************
 * Public functions
 ************************************************************************/

/************************************************************************
 * serial_initialise: set up connection
 * 
 * Parameters:
 *          struct serial *serial   Serial config structure 
 *
 * Returns:
 *      SERIAL_ERROR on error
 *      SERIAL_OK otherwise
 *
 * This function:
 *  - Sets up the I/O ports
 *  - Sets up the frame receive interrupt
 *  - Sets up & starts the timer to provide the 'clock'
 *
 * Possible errors are:
 *  - Timer already running, so UART connection already started
 *  - No PCINT interrupt possible on RX pin   
 ************************************************************************/


extern return_code_t serial_initialise(struct serial_config *config)
{

    uint32_t serial_speeds[NUM_SPEED] = {9600, 19200, 38400, 57600, 115200};

	// Sanity checks. Timer running?
	if (TCCR1 & 0x0f)
		return SERIAL_ERROR;
	
    // Setup I/O
    if (setup_io(config->tx_port, config->tx_pin, SERIAL_DIR_TX) != SERIAL_OK) {
        return SERIAL_ERROR;
    };

    if (setup_io(config->rx_port, config->rx_pin, SERIAL_DIR_RX) != SERIAL_OK) {
        return SERIAL_ERROR;
    };

    // Setup interrupt
	
	

	// Setup timer

	// Start timer. /16 prescaler - datasheet p.89 table 12-5
	TCCR1 &= ~(1 << CS13 | 1 << CS12 | 1 << CS11 | 1 << CS10);
	TCCR1 |= (1 << CS12 | 1 << CS10);

   
    connection_state = SERIAL_IDLE;

    return SERIAL_OK;

}
