/************************************************************************
 * libserial
 *
 * AVR software serial library
 ************************************************************************/

typedef enum {
	SERIAL_SPEED_9600,
	SERIAL_SPEED_19200,
	SERIAL_SPEED_38400,
	SERIAL_SPEED_57600,
	SERIAL_SPEED_115200,
} serial_speed_t;

typedef enum {
	SERIAL_NOT_INITIALISED,
	SERIAL_IDLE,
	SERIAL_SENT_START_BIT,
	SERIAL_SENDING_DATA,
	SERIAL_SENT_STOP_BIT,
	SERIAL_RECEIVED_START_BIT,
	SERIAL_RECEIVING_DATA,
	SERIAL_RECEIVED_STOP_BIT,
} serial_state_t;

struct serial_config {
	serial_speed_t speed;
	uint8_t	tx_pin;
	uint8_t tx_port;
	uint8_t	rx_pin;
	uint8_t rx_port;
};

typedef enum {
	SERIAL_ERROR,
 	SERIAL_OK,	
} return_code_t ;

/************************************************************************
 * serial_initialise: set up connection
 * 
 * Parameters:
 *          struct serial_config *config   Serial config structure 
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
 
extern return_code_t serial_initialise(struct serial_config *config);

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

extern uint8_t serial_put_char(struct serial_config *config, uint8_t data);

/************************************************************************
 * serial_send_data
 ************************************************************************/

extern uint16_t serial_send_data(struct serial_config *config, uint8_t *data, uint16_t data_length);

/************************************************************************
 * serial_data_pending
 ************************************************************************/

extern uint16_t serial_data_pending(struct serial_config *config);

/************************************************************************
 * serial_get_char     
 ************************************************************************/

extern uint8_t serial_get_char(struct serial_config *config);
