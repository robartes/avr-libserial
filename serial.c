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
#include <avr/interrupt.h>

#define NUM_SPEED		   6 
#define PRESCALER_DIVISOR   8

// Status codes
#define SERIAL_IDLE						0b00000000
#define SERIAL_SENT_START_BIT			0b00000001
#define SERIAL_SENDING_DATA				0b00000010
#define SERIAL_TX_BUFFER_LOCKED			0b00000100
#define SERIAL_RECEIVED_START_BIT		0b00001000
#define SERIAL_RECEIVING_DATA			0b00010000
#define SERIAL_RECEIVE_OVERFLOW			0b00100000

#define SERIAL_TRANSMITTING				0b00000111

#define SERIAL_NOT_INITIALISED		0b10000000



/************************************************************************
 * File global variables
 ************************************************************************/

static uint8_t connection_state = SERIAL_NOT_INITIALISED;

// Timer OCR values for clock & sample offset tresholds for RX (which are
// half the timer_ocr_values, plus some allowance for latency)
// Values below are for 8 MHz.
static uint8_t timer_ocr_values[NUM_SPEED] = {208, 52, 25, 12, 8, 3};
static uint8_t sample_offset_treshold[NUM_SPEED] = {120, 30, 14, 7, 5, 1};

struct buffer {
	uint8_t lock;
	uint8_t *data;
	uint16_t top;		// Top: points to where next byte will be written
	uint8_t dirty;
};

static volatile struct buffer rx_buffer = {0, NULL, 0, 0};
static volatile struct buffer tx_buffer = {0, NULL, 0, 0};

static volatile uint8_t rx_bit_counter = 0;
static volatile uint8_t tx_bit_counter = 0;
static volatile uint8_t rx_byte = 0;
static volatile uint8_t tx_byte = 0;
static volatile uint8_t rx_phase = 0;
static volatile uint8_t tx_phase = 0;
static volatile uint8_t rx_sample_countdown = 0;
static volatile uint8_t rx_start_bit_timecount = 0;


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
	if (port != &PORTB && port != &PINB) 
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

		if (tx_byte & (1 << bit)) {
				TX_PORT |= (1 << TX_PIN);
		} else {
				TX_PORT &= ~(1 << TX_PIN);
		}

}

/************************************************************************
 * shift_buffer_down: shift out lowest byte of a buffer, with locking
 *
 * Parameters:
 *		struct buffer *buffer	The subject buffer
 *
 * Returns:
 *		SERIAL_OK on success
 *		SERIAL_ERROR if buffer was locked
 *
 * New top+1 byte is set to 0
 ************************************************************************/

static return_code_t shift_buffer_down(volatile struct buffer *buffer)
{

	return_code_t retval = SERIAL_ERROR;
	uint8_t i = 0;

	if (!buffer->lock) {

		buffer->lock = 1;
		for (i=0; i < buffer->top ; i++) {
			buffer->data[i] = buffer->data[i+1];
		}
		buffer->data[(buffer->top)--] = 0;
		buffer->lock = 0;
		retval = SERIAL_OK;

	}

	return retval;

}

/************************************************************************
 * store_data: store a byte in the receive buffer
 *
 * Parameters:
 *		struct buffer *buffer	The receive buffer
 *
 * Returns:
 *		SERIAL_OK on success
 *		SERIAL_ERROR on failure
 * Sets SERIAL_RECEIVE_OVERFLOW on overflow
 ************************************************************************/

static return_code_t store_data(volatile struct buffer *buffer, uint8_t data)
{

	return_code_t retval = SERIAL_ERROR;

	buffer->lock = 1;

	if (buffer->top < RX_BUFFER_SIZE) {

		buffer->data[(buffer->top)++] = data;
		retval = SERIAL_OK;

	} else {

		// Drat
		move_connection_state(
			SERIAL_RECEIVING_DATA,
			SERIAL_RECEIVE_OVERFLOW
		);
	}

	buffer->lock = 0;

	return retval;

}

/************************************************************************
 * connection_state_is: check connection state
 *
 * Parameters:
 *		uint8_t expected_state	The expected connection state
 * 
 * Checks whether the serial connection state is in the state
 * expected_state
 ************************************************************************/
 
static uint8_t connection_state_is(uint8_t expected_state)
{

	return connection_state & expected_state;

}

/************************************************************************
 * (en|dis)able_rx_interrupt: start / stop RX start bit capture
 *
 * This starts/stops the pin change interrupt. It is running when no byte
 * is currently being received, to capture the next start bit
 ************************************************************************/
 
static void enable_rx_interrupt(void) {

	GIMSK |= (1 << PCIE);

}

static void disable_rx_interrupt(void) {

	GIMSK &= ~(1 << PCIE);

}

#ifndef TX_ONLY
/************************************************************************
 * Pin change interrupt 0 ISR - capture start bit for RX
 *
 * This sets rx_sample_countdown to start data bit sampling some time in 
 * the future. For Timer1 counter values smaller then half the OCR value,
 * so when we receive the start bit in the first half of a bit cycle, we
 * wait 2 timer cycles for sampling, for other case 3 timer cycles.
 * In all cases, sampling after that is every other timer cycle.
 * I should include a nice drawing of this in the documentation
 ************************************************************************/

ISR(PCINT0_vect)
{

	// Save this immediately so we know what TCNT1 is now
	// and not several clock cycles further down in the ISR
	rx_start_bit_timecount = TCNT1;

	// Sanity check. This should be a start bit, so low
	if (bit_is_set(RX_PORT, RX_PIN)) return;

	disable_rx_interrupt();

	if (rx_start_bit_timecount < sample_offset_treshold[SERIAL_SPEED]) {
		rx_sample_countdown = 2;
	} else {
		rx_sample_countdown = 3;
	}


	move_connection_state(
		SERIAL_IDLE,
		SERIAL_RECEIVED_START_BIT
	);

}
#endif

/************************************************************************
 * Timer1 Compare Match A interrupt - main polling / transmit routine
 *
 * This function is fairly large, as it does essentially all the work of 
 * this library.
 ************************************************************************/

ISR(TIM1_COMPA_vect)
{

#ifndef TX_ONLY
	// RX
	if (connection_state_is(SERIAL_RECEIVED_START_BIT)) {

		// Start sampling?
		if (rx_sample_countdown-- == 0) {
 
			// Sample first bit
			if (bit_is_set(RX_PORT, RX_PIN))
				rx_byte |= (1 << rx_bit_counter);
			rx_bit_counter++;
			rx_phase = 0;
			move_connection_state(
				SERIAL_RECEIVED_START_BIT,
				SERIAL_RECEIVING_DATA
			);

		} 

	} else if (rx_phase && connection_state_is(SERIAL_RECEIVING_DATA)) {
	
		rx_phase = 0;

		// Subsequent data bits or stop bit
		switch (rx_bit_counter) {

			case 8:

				// Stop bit. If received, load data into
				// the receive buffer. As this library is the only one
				// with access, and shifting bytes out of the buffer is 
				// done in the bottom handler of this interrupt, we can
				// be sure no one else is accessing the buffer
				if (bit_is_set(RX_PORT, RX_PIN)) {
					rx_bit_counter = 0;
					store_data(&rx_buffer, rx_byte);
					rx_byte = 0;
				} // Do nothing if this is not a stop bit
				
				// We're done with this byte, so let's wait for the next one. No rest for the wicked
				move_connection_state(
					SERIAL_RECEIVING_DATA,
					SERIAL_IDLE
				);
				enable_rx_interrupt();

				break;

			default:

				// Normal data bit
				if (bit_is_set(RX_PORT, RX_PIN))
					rx_byte |= (1 << rx_bit_counter);	
				rx_bit_counter++;

				break;
		}

	} else {

		rx_phase = 1;

	}  // if connection_state_is(SERIAL_RECEIVED_START_BIT)
#endif
	
	// TX
	switch(tx_phase) {

		case 0:

			tx_phase = 1;
			break;

		case 1: 

			tx_phase = 0;
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
					TX_PORT |=(1 << TX_PIN);

					// Try to shift out the sent byte
					if (shift_buffer_down(&tx_buffer) == SERIAL_OK) {
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
				if (shift_buffer_down(&tx_buffer) == SERIAL_OK) {
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
					TX_PORT &= ~(1 << TX_PIN);  // Start bit
					tx_byte = tx_buffer.data[0];
					tx_bit_counter = 0;
					move_connection_state(
						SERIAL_IDLE,
						SERIAL_SENT_START_BIT
					);

				}

			}

			break; // tx_phase 1

	} // switch(tx_phase)

	// Bottom handler: RX buffer 
	if (rx_buffer.dirty) {

		shift_buffer_down(&rx_buffer);
		rx_buffer.dirty = 0;

	}

}

/************************************************************************
 * acquire_buffer_lock		Buffer locking functions
 * release_buffer_lock
 *
 * Parameters:
 * 		struct buffer *buffer	The buffer to be locked / released
 * 
 * These functions lock, resp. unlock a buffer. These are not intended
 * to be called from interrupt, only from the non-interrupt functions
 ************************************************************************/

static void acquire_buffer_lock(volatile struct buffer *buffer)
{

	// Spin for lock
	while(buffer->lock);

	// Temporarily disable interrupts to make sure we are not 
	// bothered while setting the lock
	cli();
	buffer->lock = 1;
	sei();

}

static void release_buffer_lock(volatile struct buffer *buffer)
{

	// No need to disable interrupts here. Interrupts never return
	// leaving a lock of their own doing set, so no risk of accidentally
	// unlocking a buffer that should stay locked
	buffer->lock = 0;

}

/************************************************************************
 * Public functions
 ************************************************************************/

/************************************************************************
 * serial_initialise: set up connection
 * 
 * Parameters: none
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


extern return_code_t serial_initialise()
{


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
	if (setup_io(&TX_PORT, TX_PIN, SERIAL_DIR_TX) != SERIAL_OK)
		return SERIAL_ERROR;

#ifndef TX_ONLY
	if (setup_io(&RX_PORT, RX_PIN, SERIAL_DIR_RX) != SERIAL_OK)
		return SERIAL_ERROR;

	// Setup interrupt: frame receive: pin change interrupt on RX pin
	PCMSK |= (1 << RX_PIN); // Bit positions in PCMSK match pin numbers
#endif
	
	// Setup interrupt: Compare Match A interrupt Timer1
	TIMSK |= (1 << OCIE1A);	

	// Setup timer
	// CTC Mode (clear on reaching OCR1C)
	TCCR1 |= (1 << CTC1); 
	OCR1A = OCR1C = timer_ocr_values[SERIAL_SPEED];

	// Start timer. /8 prescaler - datasheet p.89 table 12-5
	TCCR1 &= ~(1 << CS13 | 1 << CS12 | 1 << CS11 | 1 << CS10);
	TCCR1 |= (1 << CS12);

	connection_state = SERIAL_IDLE;

	// Let's go
	sei();

	return SERIAL_OK;

}

/************************************************************************
 * serial_put_char: send a single byte
 *
 * Parameters:
 *		uint8_t data		Byte to send
 *
 * Returns:
 *		SERIAL_OK on success
 *		SERIAL_ERROR on failure
 *
 *
 * Please not that this function blocks on lock. However, having a TX
 * buffer locked here should not normally happen, as the interrupt that
 * could lock it does not return unless it is unlocked again.
 ************************************************************************/

extern return_code_t serial_put_char(uint8_t data)
{

	return_code_t retval = SERIAL_ERROR;

	acquire_buffer_lock(&tx_buffer);
	if (tx_buffer.top < TX_BUFFER_SIZE) {
		tx_buffer.data[(tx_buffer.top)++] = data;
		retval = SERIAL_OK;
	}
	release_buffer_lock(&tx_buffer);

	return retval;

}

/************************************************************************
 * serial_send_data: Send multiple byte serial data
 *
 * Parameters:
 *		char *data	The data to be sent
 * 		uint16_t length	The length of the data to be sent
 *
 * Returns:
 *		Number of bytes sent out. This could be less than length, as the
 *		buffer might be full or some other error might have occured
 ************************************************************************/

extern uint16_t serial_send_data(char *data, uint16_t data_length)
{

	uint16_t i;

	for (i = 0; i < data_length; i++) {

		if (serial_put_char(data[i]) == SERIAL_ERROR) 
			break;

	}

	return i;

}

static void wait_buffer_clean(volatile struct buffer *buffer) {

	// Spin on dirty buffer.
	while(buffer->dirty);

}

#ifndef TX_ONLY
/************************************************************************
 * serial_data_pending: Check whether any data has been received
 *
 * Parameters: none
 *
 * Returns: 
 *		uint16_t length	Number of bytes of data pending
 ************************************************************************/

extern uint16_t serial_data_pending()
{

	uint16_t retval = 0;

	if (rx_buffer.top) {
		wait_buffer_clean(&rx_buffer);
		retval = rx_buffer.top;
	}

	return retval;

}


/************************************************************************
 * serial_get_char: get a character from the receive buffer
 *
 * Parameters: none
 *
 * Returns: 
 *		uint8_t	data	The data retrieved from the buffer
 ************************************************************************/

extern uint8_t serial_get_char()
{

	uint8_t my_data;

	wait_buffer_clean(&rx_buffer);

	my_data = rx_buffer.data[0];  	// FIFO: always read from the front
	rx_buffer.dirty = 1;		// Signal bottom handler to shift data

	return my_data;

}

/************************************************************************
 * (en|dis)able_receive: start or stop listening for incoming data
 *
 * Parameters: none
 *
 * Returns: nothing
 ************************************************************************/

extern void serial_enable_receive()
{

	enable_rx_interrupt();

}

extern void serial_disable_receive()
{

	disable_rx_interrupt();

}
#endif
