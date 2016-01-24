/************************************************************************
 * libserial
 *
 * AVR software serial library for ATTinyx5
 ************************************************************************/

#include <avr/io.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "serial.h"

#define NUM_SPEED		   5 
#define PRESCALAR_DIVISOR   16

#ifndef F_CPU
#define F_CPU 20000000
#endif

/************************************************************************
 * File global variables
 ************************************************************************/

static uint8_t connection_state = SERIAL_NOT_INITIALISED;

struct buffer {
	uint8_t lock;
	uint8_t *data;
	uint8_t top;
	uint8_t dirty;
};

static struct buffer rx_buffer = {0, NULL, 0, 0};
static struct buffer tx_buffer = {0, NULL, 0, 0};

static struct serial_config my_config;

volatile uint8_t rx_bit_counter = 0;
volatile uint8_t tx_bit_counter = 0;
volatile uint8_t rx_byte = 0;
volatile uint8_t tx_byte = 0;


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
 * move_connection_state: change connection state to a new state
 *
 * Parameters:
 *		uint8_t	src_state	State to move from
 *		uint8_t	dst_state	State to move to
 * 
 * Actual state values are defined in serial.h
 ************************************************************************/

static void move_connection_state(uint8_t src_state, uint8_t dst_state)
{

	connection_state &= ~src_state;
	connection_state |= dst_state;

}

/************************************************************************
 * send_data_bit: set the tx pin to a new data bit
 *
 * Parameters:
 *		uint8_t	bit	The data bit to send
 ************************************************************************/

static void send_data_bit(uint8_t bit)
{

		switch(tx_byte & (1 << bit)) {
			case 1:
				*(my_config.tx_port) |= (1 << my_config.tx_pin);
				break;
			case 0:
				*(my_config.tx_port) &= ~(1 << my_config.tx_pin);
				break;

		}

}

/************************************************************************
 * shift_buffer_down: shift out lowest byte of a buffer, with locking
 *
 * Parameters:
 *		struct buffer *buffer	The subject buffer
 *
 * Returns:
 *		1 on success
 *		0 if buffer was locked
 *
 * New top+1 byte is set to 0
 ************************************************************************/

uint8_t shift_buffer_down(struct buffer *buffer)
{

	uint8_t retval = 0;
	uint8_t i = 0;

	if (!buffer->lock) {

		buffer->lock = 1;
		for (i=0; i < buffer->top ; i++) {
			buffer->data[i] = buffer->data[i+1];
		}
		buffer->data[(buffer->top)--] = 0;
		buffer->lock = 0;
		retval = 1;

	}

	return retval;

}

static void store_data(struct buffer *buffer, uint8_t data)
{

	buffer->lock = 1;

	if (buffer->top < RX_BUFFER_SIZE) {

		buffer->data[(buffer->top)++] = data;

	} else {

		// Drat
		(buffer->top)--;
		move_connection_state(
			SERIAL_RECEIVING_DATA,
			SERIAL_RECEIVE_OVERFLOW
		);
	}

	buffer->lock = 0;

}

static uint8_t connection_state_is(uint8_t expected_state)
{

	return connection_state & expected_state;

}

/************************************************************************
 * Timer1 Compare Match A interrupt - main polling / transmit routine
 *
 * This function is fairly large, as it does essentially all the work of 
 * this library.
 ************************************************************************/

ISR(TIM1_COMPA_vect)
{

	// RX
	if (connection_state_is(SERIAL_RECEIVED_START_BIT)) {

		// First data bit
		if (bit_is_set(*(my_config.rx_port), my_config.rx_pin))
			rx_byte |= (1 << rx_bit_counter);	
		rx_bit_counter++;
		move_connection_state(
			SERIAL_RECEIVED_START_BIT,
			SERIAL_RECEIVING_DATA 
		);
			

	} else if (connection_state_is(SERIAL_RECEIVING_DATA)) {
	
		// Subsequent data bits or stop bit
		switch (rx_bit_counter) {

			case 8:

				// Stop bit. If received, load data into
				// the receive buffer. As this library is the only one
				// with access, and shifting bytes out of the buffer is 
				// done in the bottom handler of this interrupt, we can
				// be sure no one else is accessing the buffer
				if (bit_is_set(*(my_config.rx_port), my_config.rx_pin)) {
					rx_bit_counter = 0;
					store_data(&rx_buffer, rx_byte);
					rx_byte = 0;
					move_connection_state(
						SERIAL_RECEIVING_DATA,
						SERIAL_IDLE
					);
				} else {
					// Weird - no stop bit after 8 data bits. Reset connection
					move_connection_state(
						SERIAL_RECEIVING_DATA,
						SERIAL_IDLE
					);

				}
				break;

			default:

				// Normal data bit
				if (bit_is_set(*(my_config.rx_port), my_config.rx_pin))
					rx_byte |= (1 << rx_bit_counter);	
				rx_bit_counter++;

				break;
		}

	} else {

		// Not receiving anything right now. Check for start bit
		if (bit_is_clear(*(my_config.rx_port), my_config.rx_pin))
			move_connection_state(
				SERIAL_IDLE,
				SERIAL_RECEIVED_START_BIT
			);

	}


	
	// TX
	if (connection_state_is(SERIAL_SENT_START_BIT)) {

		// Write first bit of data
		send_data_bit(tx_bit_counter++);
		move_connection_state(
			SERIAL_SENT_START_BIT,
			SERIAL_SENDING_DATA 
		);

	} else if (connection_state_is(SERIAL_SENDING_DATA)) {

		// Data or stop bit
		if (tx_bit_counter == 8) {

			// Stop bit
			*(my_config.tx_port) |=(1 << my_config.tx_pin);

			// Try to shift out the sent byte
			if (shift_buffer_down(&tx_buffer)) {
				move_connection_state(
					SERIAL_SENDING_DATA,
					SERIAL_IDLE
				);
			} else {
				// Drat. Try again later
				move_connection_state(
					SERIAL_SENDING_DATA,
					SERIAL_TX_BUFFER_LOCKED
				);
			}

		} else {

			// Data bit
			send_data_bit(tx_bit_counter++);

		}

	} else if (connection_state_is(SERIAL_TX_BUFFER_LOCKED)) {

		// Keep trying to shift TX buffer
		if (shift_buffer_down(&tx_buffer)) {
			move_connection_state(
				SERIAL_SENDING_DATA,
				SERIAL_IDLE
			);
		}	

	} else {

		// Not sending anything. Check for byte to send. Not using dirty
		// flag as top != 0 means the same thing for TX buffer
		if (tx_buffer.top) {

			// New data
			*(my_config.tx_port) &= ~(1 << my_config.tx_pin);  // Start bit
			tx_byte = tx_buffer.data[0];
			tx_bit_counter = 0;
			move_connection_state(
				SERIAL_IDLE,
				SERIAL_SENT_START_BIT
			);

		}

	}


	// Bottom handler: RX buffer 
	if (rx_buffer.dirty) {

		shift_buffer_down(&rx_buffer);
		rx_buffer.dirty = 0;

	}

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

	if ((txd = malloc(TX_BUFFER_SIZE)) == NULL)
		return SERIAL_ERROR;

	rx_buffer.data = rxd;
	tx_buffer.data = txd;

	// Setup I/O
	if (setup_io(config->tx_port, config->tx_pin, SERIAL_DIR_TX) != SERIAL_OK)
		return SERIAL_ERROR;

	if (setup_io(config->rx_port, config->rx_pin, SERIAL_DIR_RX) != SERIAL_OK)
		return SERIAL_ERROR;

	my_config.rx_port = config->rx_port;
	my_config.rx_pin = config->rx_pin;
	my_config.tx_port = config->tx_port;
	my_config.tx_pin = config->tx_pin;

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


