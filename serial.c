/************************************************************************
 * libserial
 *
 * AVR software serial library
 ************************************************************************/

#include <avr/io.h>
#include <stdint.h>

#define NUM_SPEED   sizeof(serial_speed_t)

/************************************************************************
 * state variables (global)
 ************************************************************************/

serial_state_t connection_state = NOT_INITIALISED;

/************************************************************************
 * Private functions
 ************************************************************************/

static return_code_t setup_io((volatile uint8_t *) port, (volatile uint8_t *) pin)
{


    return OK;

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
 *      ERROR on error
 *      OK otherwise
 *
 * This function:
 *  - Starts the timer to provide the 'clock'
 *  - Sets up the I/O ports
 *  - Sets up the frame receive interrupt
 *
 * Possible errors are:
 *  - Timer already running, so UART connection already started
 *  - No PCINT interrupt possible on RX pin   
 ************************************************************************/

/************************************************************************
 * Developer information on timer frequency
 *
 * The limits on timer ISR frequency are determined by the upper and 
 * lower bound on baud rate: 115200 and 9600 baud, respectively.
 * The timer needs to be fast enough to trigger 115200 times per second,
 * yet slow enough to also be able to trigger only 9600 times per second.
 * If we run the numbers on this, we get:
 *
 *      Ftimer_max < 9600.256 = 2.457.600 Hz
 *      Ftimer_min > 115200.2 = 230.400 Hz
 *
 * If the timer is faster then Ftimer_max, it cannot count as slow as 
 * 9600 Hz any more (these are 8-bit timers, so can only count to 256).
 * If it is slower than Ftimer_min, it is not fast enough to count to
 * 115200 Hz any more (as the minimum OCR value is 1, and the ISR
 * triggers at the *end* of the timer cycle).
 *
 * Timer1 on an ATTinyx5 has a nice /16 prescaler mode which fits our
 * range perfectly. At 4 MHz (I'm assuming you are not going to clock
 * your ATTiny slower than that), we have a frequency of 250kHz, while
 * at 20Mhz (datasheet maximum) it will be 1.25MHz. 
 *
 * Please note that the CPU frequency is determined at compile
 * time through the F_CPU macro. If you play with the clock prescaler
 * at runtime, serial timings will be off.
 ************************************************************************/

extern return_code_t serial_initialise(struct serial_config *config)
{

    uint16_t serial_speeds[NUM_SPEED] = {9600, 19200, 38400, 57600, 115200};

    // Setup I/O
    if (setup_io(config->tx_port, config->tx_pin) != OK) {
        return ERROR;
    };

    if (setup_io(config->rx_port, config->rx_pin) != OK) {
        return ERROR;
    };

    // Setup interrupt

	// Start timer. /16 prescaler - datasheet p.89 table 12-5
	TCCR1 &= ~(1 << CS13 | 1 << CS12 | 1 << CS11 | 1 << CS10);
	TCCR1 |= (1 << CS12 | 1 << CS10);

   
    connection_state = IDLE;

    return OK;

}
