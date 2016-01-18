/************************************************************************
 * libserial
 *
 * AVR software serial library
 ************************************************************************/


/************************************************************************
 * serial_initialise: set up connection
 * 
 * Parameters:
 *          struct serial *serial   Serial config structure 
 *
 * Returns:
 *      null on error
 *      The serial config structure with state filled in on OK
 *
 * This function:
 *  - Starts the timer to provide the 'clock'
 *  - Sets up the I/O ports
 *  - Sets up the frame receive interrupt
 * Possible errors are:
 *  - Timer already running, so UART connection already started
 *  - No PCINT interrupt possible on RX pin   
 *
 *  Developer information on timer frequency
 *
 *  The limits on timer ISR frequency are determined by the upper and 
 *  lower bound on baud rate: 115200 and 9600 baud, respectively.
 *  The timer needs to be fast enough to trigger 115200 times per second,
 *  yet slow enough to also be able to trigger only 9600 times per second.
 *  If we run the numbers on this, we get:
 *
 *      Ftimer_max < 9600.256 = 2.457.600 Hz
 *      Ftimer_min > 115200.2 = 330.400 Hz
 *
 * If the timer is faster then Ftimer_max, it cannot count as slow as 
 * 9600 Hz any more (these are 8-bit timers, so can only count to 256).
 * If it is slower than Ftimer_min, it is not fast enough to count to
 * at 115200 Hz any more (as the minimum OCR value is 1, and the ISR
 * triggers at the *end* of the timer cycle).
 *
 * So, for F_CPU under 4000000, we use the system clock without prescaler
 * and run the timer at CLKI/O. For F_CPU under 16000000 we run the timer
 * with /8 prescaler, so timer frequency is between 500kHz and 2MHz. So
 * far so good.
 *
 * Above F_CPU 16000000 we run into a problem, however. The next
 * prescaler step is /64. For CPU speeds between 16MHz and 20MHz this
 * would result in the timer frequency to be too low for the highest 
 * baud rate. If we stick with the /8 prescaler it would be too high for 
 * the lowest baud rate as of 19.66MHz.
 *
 * There are a few possible solutions for this:
 *
 * 1. Use /8 prescaler and 'count extra' for the lowest baud rates:
 *    have the ISR increment a counter, and only check data when 
 *    that counter reaches a certain value, so we are effectively
 *    counting to 256 several times. This introduces programming
 *    complexity
 * 2. Use /64 prescaler and ditch the highest baud rate
 * 3. Use /8 prescaler and ditch the lowest baud rate
 *
 * We opt for option 1
 *
 * Please also note that the CPU frequency is determined at compile
 * time through the F_CPU macro. If you play with the clock prescaler
 * at runtime, serial timings will be off. Please also note that the 
 * below timings are only valid until around 39Mhz when we need a third
 * time around the timer for 9600 and a second time around for 19200. If
 * you want to overclock your AVR, please modify the code accordingly
 ************************************************************************/

static struct _serial_timing {
    uint8_t high_speed_clock;
    uint8_t low_baud_extra_count;
};
  
// Defined as a global variable. Alternative is to add a void * for this
// to struct serial_data, but that's kludgy. Global variables are kludgy
// too, but I'd rather be kludgy in the private part than the public part
// of the library
static struct _serial_timing timing_data={0, 0, 0};

extern struct serial *serial_initialise(struct serial *serial)
{

    // Setup timing data
    
#if F_CPU < 4000001
    // All values of timing_data default
#else 
    timing_data.high_speed_clock=1;
#if F_CPU > 19660800  
    timing_data.low_baud_extra_count=1;
#endif  // F_CPU > 19660800
#endif // F_CPU < 4000001

    // Setup I/O
    // Setup interrupt
     // Start timer

#if F_CPU < 4000001
    // CLK I/O
    TCCR0B &= ~(1 << CS02 | 1 << CS01 | 1 << CS00);
    TCCR0B |= (1 << CS00);
#else
    // CLK I/O / 8
    TCCR0B &= ~(1 << CS02 | 1 << CS01 | 1 << CS00);
    TCCR0B |= (1 << CS01);
#endif

   
    serial->state = IDLE;

    return serial;

}
