/************************************************************************
 * libserial
 *
 * AVR software serial library
 ************************************************************************/

#define SERIAL_SPEED_2400	0
#define SERIAL_SPEED_9600	1
#define SERIAL_SPEED_19200	2
#define SERIAL_SPEED_38400	3
#define SERIAL_SPEED_57600	4
#define SERIAL_SPEED_115200	5

#define	TX_PORT						PORTB
#define TX_PIN						PB1
#define	RX_PORT						PINB
#define	RX_PIN						PB2
#define SERIAL_SPEED				SERIAL_SPEED_9600
#define RX_BUFFER_SIZE				64			// In bytes
#define TX_BUFFER_SIZE				64			// In bytes

typedef enum {
	SERIAL_ERROR,
	SERIAL_OK,	
} return_code_t ;

/************************************************************************
 * serial_initialise: set up connection
 * 
 * Parameters: none
 *
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
 
extern return_code_t serial_initialise();

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
 * 		uint16_t length	The length of the data to be sent
 *
 * Returns:
 *		Number of bytes sent. This could be less than length, as the
 *		buffer might be full or some other error might have occured
 ************************************************************************/

extern uint16_t serial_send_data(char *data, uint16_t data_length);

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
