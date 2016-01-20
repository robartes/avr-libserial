/************************************************************************
 * libserial
 *
 * AVR software serial library
 ************************************************************************/

#include <avr/io.h>
#include <stdint.h>

#define NUM_SPEED   sizeof(serial_speed_t)

// libserial normally uses Timer0, but at compile time can be configured
// to use Timer1 by defining LIBSERIAL_USE_TIMER1
#ifdef LIBSERIAL_USE_TIMER1
    (volatile int *) timer_ocr = &OCR1A;        // Not in __assembler__ so need &(SFR_REG).
    (volatile int *) timer_tccra = &TCCR1A;
    (volatile int *) timer_tccrb = &TCCR1B;
#else
    (volatile int *) timer_ocr = &OCR0A;
    (volatile int *) timer_tccra = &TCCR0A;
    (volatile int *) timer_tccrb = &TCCR0B;
#endif

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
 * So, for F_CPU under 4000000, we use the system clock without prescaler
 * and run the timer at CLKI/O. For F_CPU under 16000000 we run the timer
 * with /8 prescaler, so timer frequency is between 500kHz and 2MHz. So
 * far so good. 
 *
 * At over 16000000 MHz we switch to the next prescaler value, which is
 * /64. Timer frequency will be between 250kHz and 312.5kHz (assuming we
 * stop at 20MHz), which falls within our range.
 *
 * Please note that the CPU frequency is determined at compile
 * time through the F_CPU macro. If you play with the clock prescaler
 * at runtime, serial timings will be off. Please also note that the 
 * below timings are only valid until around 39Mhz when we need a third
 * time around the timer for 9600 and a second time around for 19200. If
 * you want to overclock your AVR, please modify the code accordingly
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
	// Start timer with appropriate prescaler

#   if F_CPU < 4000000

    // CLK I/O
    timer_tccrb &= ~(1 << CS02 | 1 << CS01 | 1 << CS00);
    timer_tccrb |= (1 << CS00);

#   elif F_CPU < 16000000

    // CLK I/O / 8
    timer_tccrb &= ~(1 << CS02 | 1 << CS01 | 1 << CS00);
    timer_tccrb |= (1 << CS01);

#   else

	// CLK I/O / 64
    timer_tccrb &= ~(1 << CS02 | 1 << CS01 | 1 << CS00);
    timer_tccrb |= (1 << CS01 | 1 << CS00);

#   endif

   
    connection_state = IDLE;

    return OK;

}
