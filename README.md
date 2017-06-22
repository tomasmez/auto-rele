# auto-rele
timer service with calendar for a raspberry pi relay addon board

It is a systemd daemon which runs a calendar for 3 outputs
of the gpio pins of a raspberry pi.

it relies on wiringpi for managing the gpios and 
on the iniparser library for configuration.

The configuration is self explanatory.
the relay will turn on between on_time and off_time and remain
off otherwise.

it supports a manual pin input which toggles the output.

if a manual pin sets the relay on during an off time. it stays
on until the next off_time deadline.

if a maual pin sets the relay off during an on time. it stays
off until the next on_time deadline.

