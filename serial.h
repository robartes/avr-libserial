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
	IDLE,
	SENT_START_BIT,
	SENDING_DATA,
	SENT_STOP_BIT,
	RECEIVED_START_BIT,
	RECEIVING_DATA,
	RECEIVED_STOP_BIT,
} serial_status_t;

struct serial {
	serial_speed_t speed;
	uint8_t	tx_pin;
	uint8_t tx_port;
	uint8_t	rx_pin;
	uint8_t rx_port;
	serial_status_t status;
	
};

/************************************************************************
 * serial_put_char: send a single byte
 *
 * Parameters:
 * 		struct serial *		Configuration structure of this connection
 *		uint8_t data		Byte to send
 *
 * Returns:
 * 		uint8_t length 			Length of data sent out (should be 1)
 ************************************************************************/

extern uint8_t serial_put_char(struct serial *serial, uint8_t data);

/************************************************************************
 * serial_send_data
 ************************************************************************/

extern uint16_t serial_send_data(struct serial *serial, uint8_t *data, uint16_t data_length);

/************************************************************************
 * serial_data_pending
 ************************************************************************/

extern uint16_t serial_data_pending(struct serial *serial);

/************************************************************************
 * serial_get_char     
 ************************************************************************/

extern uint8_t serial_get_char(struct_serial *serial);
