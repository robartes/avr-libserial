/************************************************************************
 * libserial
 *
 * AVR software serial library
 ************************************************************************/

#define RX_BUFFER_SIZE				64			// In bytes
#define TX_BUFFER_SIZE				64			// In bytes

typedef enum {
	SERIAL_ERROR,
	SERIAL_OK,	
} return_code_t ;

typedef enum {
	SERIAL_SPEED_2400,
	SERIAL_SPEED_9600,
	SERIAL_SPEED_19200,
	SERIAL_SPEED_38400,
	SERIAL_SPEED_57600,
	SERIAL_SPEED_115200,
} serial_speed_t;

/************************************************************************
 * struct serial_init: initialisation structure
 *
 * Members:
 *		char *rx_pin	Pin for receive (standard Pxy format)
 *		char *tx_pin	Pin for transmit (standard Pxy format)
 *		serial_speed_t speed	Serial speed
 ************************************************************************/

struct serial_init {
	char *rx_pin;
	char *tx_pin;
	serial_speed_t speed;
};

/************************************************************************
 * serial_initialise: set up connection
 * 
 * Parameters: struct serial_init *
 *			Initialisation structure
 * Returns:
 *	  ERROR on error
 *	  OK otherwise
 *
 * This function:
 *  - Mallocs RX & TX buffers
 *  - Starts the timer to provide the 'clock'
 *  - Sets up the I/O ports
 *  - Sets up the frame receive interrupt
 * Possible errors are:
 *  - Not enough memory for buffers
 *  - Timer already running, so UART connection already started
 *  - No PCINT interrupt possible on RX pin   
 ************************************************************************/
 
extern return_code_t serial_initialise(struct serial_init*);

/************************************************************************
 * serial_put_char: send a single byte
 *
 * Parameters:
 *		uint8_t data		Byte to send
 *
 * Returns:
 *		SERIAL_OK on success
 *		SERIAL_ERROR on failure
 ************************************************************************/

extern return_code_t serial_put_char(uint8_t data);

/************************************************************************
 * serial_send_data: Send multiple byte serial data
 *
 * Parameters:
 *		uint8_t *data	The data to be sent
 *
 * Returns:
 *		Number of bytes sent. This could be less than length, as the
 *		buffer might be full or some other error might have occured
 ************************************************************************/

extern uint16_t serial_send_data(char *data);

#ifndef TX_ONLY
/************************************************************************
 * serial_data_pending: Check whether any data has been received
 *
 * Parameters: none
 *
 * Returns: 
 *		uint16_t length	Number of bytes of data pending
 ************************************************************************/

extern uint16_t serial_data_pending();

/************************************************************************
 * serial_get_char: get a character from the receive buffer
 *
 * Parameters: none
 *
 * Returns: 
 *		uint8_t	data	The data retrieved from the buffer
 ************************************************************************/

extern uint8_t serial_get_char();

/************************************************************************
 * (en|dis)able_receive: start or stop listening for incoming data
 *
 * Parameters: none
 *
 * Returns: nothing
 ************************************************************************/

extern void serial_enable_receive();
extern void serial_disable_receive();
#endif
