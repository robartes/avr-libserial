/************************************************************************
 * libserial
 *
 * AVR software serial library
 ************************************************************************/

#define SERIAL_IDLE						0b00000000
#define SERIAL_SENT_START_BIT			0b00000001
#define SERIAL_SENDING_DATA				0b00000010
#define SERIAL_SENT_STOP_BIT			0b00000100
#define SERIAL_RECEIVED_START_BIT		0b00001000
#define SERIAL_RECEIVING_DATA			0b00010000
#define SERIAL_RECEIVE_OVERFLOW			0b00100000

#define SERIAL_TRANSMITTING				0b00000111

#define SERIAL_NOT_INITIALISED		0b10000000

#define RX_BUFFER_SIZE				64			// In bytes
#define TX_BUFFER_SIZE				64			// In bytes

typedef enum {
	SERIAL_SPEED_9600,
	SERIAL_SPEED_19200,
	SERIAL_SPEED_38400,
	SERIAL_SPEED_57600,
	SERIAL_SPEED_115200,
} serial_speed_t;


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
 *		  struct serial_config *config   Serial config structure 
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
 
extern return_code_t serial_initialise(struct serial_config *config);

/************************************************************************
 * serial_put_char: send a single byte
 *
 * Parameters:
 *		struct serial_config *		Configuration structure of this connection
 *		uint8_t data		Byte to send
 *
 * Returns:
 *		uint8_t length			Length of data sent out (should be 1)
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
