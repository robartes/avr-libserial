== Timer frequency
 
The limits on timer ISR frequency are determined by the upper and 
lower bound on baud rate: 115200 and 9600 baud, respectively.
The timer needs to be fast enough to trigger 115200 times per second,
yet slow enough to also be able to trigger only 9600 times per second.
If we run the numbers on this, we get:

     Ftimer_max < 9600.256 = 2.457.600 Hz
     Ftimer_min > 115200.2 = 230.400 Hz
If the timer is faster then Ftimer_max, it cannot count as slow as 
9600 Hz any more (these are 8-bit timers, so can only count to 256).
If it is slower than Ftimer_min, it is not fast enough to count to
115200 Hz any more (as the minimum OCR value is 1, and the ISR
triggers at the *end* of the timer cycle).

Timer1 on an ATTinyx5 has a nice /16 prescaler mode which fits our
range perfectly. At 4 MHz (I'm assuming you are not going to clock
your ATTiny slower than that), we have a frequency of 250kHz, while
at 20Mhz (datasheet maximum) it will be 1.25MHz. 

Please note that the CPU frequency is determined at compile
time through the F_CPU macro. If you play with the clock prescaler
at runtime, serial timings will be off.

== Full duplex with 1 timer

To achieve full duplex operation with 1 timer only we have to look at 
what happens when incoming data (hereafter referred to as RX) arrives
while data is being sent out (herafter referred to as TX). As we do
not control when RX arrives, it is totally asynchronous with the timer
running and could arrive at any time in the timer cycle.

One way of dealing with the asynchronous nature of RX is to trigger an
interrupt on arrival of a start bit. The Atmel application note AVR304
works like this. The problem with this approach is that the first timer
wait cycle, between the arrival of the start bit (the falling edge of 
the RX line) and the first data bit needs to be 1.5 bit times to
ensure data sampling is done in the middle of a bit and not at the edges.
Unfortunately, we cannot use this approach as with full duplex, a TX
may be ongoing, which uses the same timer and needs 1 bit time wait times
to ensure correct sending of data. 

There are various possible ways of dealing with this:

    1. use half duplex, and have arriving RX stopping ongoing TX
    2. use 2 timers, one for RX and one for TX
    3. run the timer at double the data frequency
    4. do not use interrupt to start RX but poll RX line in timer ISR

1 and 2 violate our premise (full duplex with one timer), and 3 would mean
that with our chosen prescaler the timer would not be fast enough at the
lowest I/O clock speeds (4MHz). So we opt for option 4.

The risk with polling is of course that we poll in the middle of a rising
edge at the end of a start bit. This will lead to corrupted data or no
data at all if data bits are all 1. However, the input pin synchronisation
mechanism (datasheet p.55 section 10.2.4) takes care of avoiding metastable
inputs so we can be sure to either get a 1 or a 0. If the transmitter and
receiver clocks are sufficiently in sync, this will ensure we will catch 
the start bit. I hope.


