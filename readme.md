avr-libserial is a simple UART library for ATTinyx5 devices. It uses one
timer (Timer1) and supports full duplex communication.

It currently has hardcoded magic timer compare match values for an 8MHz
clock, and has only really been tested for 9600 baud.

In the spirit of 'release early and often' I'm throwing this out there. 
I might update it later to calculate the magic values based on clock speed. 
Or I might not :)
