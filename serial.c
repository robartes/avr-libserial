/************************************************************************
 * libserial
 *
 * AVR software serial library for ATTinyx5
 ************************************************************************/

#include <avr/io.h>
#include <stdint.h>
#include "serial.h"

#define NUM_SPEED		   5 
#define PRESCALER_DIVISOR   16

/************************************************************************
 * File global variables
 ************************************************************************/

static uint8_t connection_state = SERIAL_NOT_INITIALISED;

struct {
	uint8_t lock;
	uint8_t *data;
	uint8_t top;
} buffer;

static buffer rx_buffer = {0, NULL, 0};
static buffer tx_buffer = {0, NULL, 0};

static struct serial_config my_config;

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

static void move_connection_state(uint8_t src_state, uint8_t dst_state)
{

	connection_state &= ~src_state;
	connection_state |= dst_state;

}

/************************************************************************
 * Timer1 Compare Match A interrupt - main polling / transmit routine
 ************************************************************************/

volatile uint8_t rx_bit_counter = 0;
volatile uint8_t tx_bit_counter = 0;
volatile uint8_t rx_byte = 0;

ISR(TIM1_COMPA_vect)
{

	// RX
	if (get_connection_state(SERIAL_RECEIVED_START_BIT)) {

		// First data bit

		if (bit_is_set(*rx_port, rx_pin))
			rx_byte |= (1 << rx_bit_counter);	
		rx_bit_counter++;
		move_connection_state(
			SERIAL_RECEIVED_START_BIT,
			SERIAL_RECEIVING_DATA 
		);
			

	} else if (get_connection_state(SERIAL_RECEIVING_DATA)) {
	
		// Subsequent data bits or stop bit
		switch (rx_byte_counter) {

			case 8:

				// Check for stop bit
				if (bit_is_set(*rx_port, rx_pin)) {
					rx_bit_counter = 0;
					if (!rx_buffer.lock) {
						store_data(*rx_buffer, rx_byte);
						rx_byte = 0;
						move_connection_state(
							SERIAL_RECEIVING_DATA,
							SERIAL_IDLE,
						);
					} else {
						// Buffer was locked. Try again
						move_connection_state(
							SERIAL_RECEIVING_DATA,
							SERIAL_RECEIVE_BUFFER_LOCKED,
						);
					}
				} else {
					// Weird - no stop bit after 8 data bits. Reset connection
					move_connection_state(
						SERIAL_RECEIVING_DATA,
						SERIAL_IDLE,
					);

				}
				break;

			default:

				break;
		}

    } else if (get_connection_state(SERIAL_RECEIVED_STOP_BIT)) {

	} else {

		// Not receiving anything right now. Check for start bit
		if (bit_is_clear(*rx_port, rx_pin))
			set_connection_state(SERIAL_RECEIVED_START_BIT);

	}


	
	// TX
}

/************************************************************************
 * Public functions
 ************************************************************************/

/************************************************************************
 * serial_initialise: set up connection
 * 
 * Parameters:
 *		  struct serial *serial   Serial config structure 
 *
 * Returns:
 *	  SERIAL_ERROR on error
 *	  SERIAL_OK otherwise
 *
 * This function:
 *  - Mallocs rx & tx buffers
 *  - Sets up the I/O ports
 *  - Sets up the frame receive interrupt
 *  - Sets up & starts the timer to provide the 'clock'
 *
 * Possible errors are:
 * 	- Not enough memory for buffers
 *  - Timer already running, so UART connection already started
 *  - No PCINT interrupt possible on RX pin   
 ************************************************************************/


extern return_code_t serial_initialise(struct serial_config *config)
{

	uint32_t serial_speeds[NUM_SPEED] = {9600, 19200, 38400, 57600, 115200};
	uint8_t *rxd;
	uint8_t *txd;


	// Sanity checks. Timer running?
	if (TCCR1 & 0x0f)
		return SERIAL_ERROR;

	// Allocate buffers
	if ((rxd = malloc(RX_BUFFER_SIZE)) == NULL)
		return SERIAL_ERROR;

	if ((tx = malloc(TX_BUFFER_SIZE)) == NULL)
		return SERIAL_ERROR;

	rx_buffer.data = rxd;
	tx_buffer.data = txd;

	// Setup I/O
	if (setup_io(config->tx_port, config->tx_pin, SERIAL_DIR_TX) != SERIAL_OK) {
		return SERIAL_ERROR;
	};

	if (setup_io(config->rx_port, config->rx_pin, SERIAL_DIR_RX) != SERIAL_OK) {
		return SERIAL_ERROR;
	};

	my_config.rx_port = rx_port;
	my_config.rx_pin = rx_pin;
	my_config.tx_port = tx_port;
	my_config.tx_pin = tx_pin;

	// Setup interrupt: Compare Match A interrupt Timer1
	TIMSK |= (1 << OCIE1A);	

	// Setup timer
	// CTC Mode (clear on reaching OCR1C)
	TCCR1 |= (1 << CTC1); 
	OCR1A = OCR1C = (uint8_t) (F_CPU / PRESCALAR_DIVISOR / serial_speeds[config->speed]) - 1;

	// Start timer. /16 prescaler - datasheet p.89 table 12-5
	TCCR1 &= ~(1 << CS13 | 1 << CS12 | 1 << CS11 | 1 << CS10);
	TCCR1 |= (1 << CS12 | 1 << CS10);

	connection_state = SERIAL_IDLE;

	return SERIAL_OK;

}

extern uint8_t get_connection_state(uint8_t expected_state)
{

	return connection_state & expected_state;

}

