/************************************************************************
 * libserial
 *
 * AVR software serial library
 ************************************************************************/

typedef enum {
	9600,
	19200,
	38400,
	57600,
	115200,
} serial_speed_t;

typedef enum {
	NOT_INITIALISED,
	IDLE,
	SENT_START_BIT,
	SENDING_DATA,
	SENT_STOP_BIT,
	RECEIVED_START_BIT,
	RECEIVING_DATA,
	RECEIVED_STOP_BIT,
} serial_state_t;

struct serial_config {
	serial_speed_t speed;
	uint8_t	tx_pin;
	uint8_t tx_port;
	uint8_t	rx_pin;
	uint8_t rx_port;
};

typedef enum {
	ERROR,
 	OK,	
} return_code_t ;

/************************************************************************
 * serial_initialise: set up connection
 * 
 * Parameters:
 *          struct serial_config *serial   Serial config structure 
 *
 * Returns:
 *      ERROR on error
 *      OK otherwise
 *
 * This function:
 *  - Starts the timer to provide the 'clock'
 *  - Sets up the I/O ports
 *  - Sets up the frame receive interrupt
 * Possible errors are:
 *  - Timer already running, so UART connection already started
 *  - No PCINT interrupt possible on RX pin   
 ************************************************************************/
 
extern return_code_t serial_initialise(struct serial_config *serial);

/************************************************************************
 * serial_put_char: send a single byte
 *
 * Parameters:
 * 		struct serial_config *		Configuration structure of this connection
 *		uint8_t data		Byte to send
 *
 * Returns:
 * 		uint8_t length 			Length of data sent out (should be 1)
 ************************************************************************/

extern uint8_t serial_put_char(struct serial_config *serial, uint8_t data);

/************************************************************************
 * serial_send_data
 ************************************************************************/

extern uint16_t serial_send_data(struct serial_config *serial, uint8_t *data, uint16_t data_length);

/************************************************************************
 * serial_data_pending
 ************************************************************************/

extern uint16_t serial_data_pending(struct serial_config *serial);

/************************************************************************
 * serial_get_char     
 ************************************************************************/

extern uint8_t serial_get_char(struct_serial *serial);
