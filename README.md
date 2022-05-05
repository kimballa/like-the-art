
# You Don't Have To Like All The Art!

An art project, built on Arduino.

This sketch manages power distribution to a collection of LED signs that spell
sentences when lit, and allows the viewer some semblance of control through
a collection of buttons on a controller box.

## LED Neon Power

Power is distributed through a collection of IRBL8721 Power MOSFETs. We
can activate them through our I/O which is a combination of GPIO and 8-bit
parallel busses (PCF8574) on I2C. These signals are AND'd (by a SN75374, which drives
the MOSFETs) with a PWM signal we also control for brightness and effects control.

The '374 is negative logic; we drive the input low to illuminate the LEDs.

## Ambient operation

By default, the sign will rotate through a carousel of messages and change the active effect
for each message.

## Button inputs

A 3x3 button grid offers user input.

* Some buttons change the active message
* Other buttons change the active effect

When a user presses a button that selects a message, that message will be displayed for five
seconds, and then go back to the carousel.

When a user presses a button that selects an active effect, that effect will persist on all
messages for the next 15 seconds before reverting to random.

With low probability, every button press may scramble which buttons map to which operation
(message select or effect select).

## Debug mode

The buttons form a grid much like a telephone number pad:

```
1 2 3
4 5 6
7 8 9
```

Recent button inputs are recorded in a shift register. When an access code is entered, the
device switches into debug mode.

The first LED sign will begin blinking when debug mode is active.

In this mode, the buttons have the following operations:

* 1 - Full sign test. Light up each sign in series. Repeats indefinitely until a new
  operation is chosen.
* 2 - Light up prev sign. Don't auto-progress.
* 3 - Light up next sign. Don't auto-progress.
* 4 - Use next available effect.
* 5 - Light up prev sentence/message.
* 6 - Light up prev sentence/message.
* 7 - Hold 1 second to exit debug mode. The first 3 signs flash 3 times and then
  debug mode ends.
* 8 - Toggle between three max brightness levels for power management: 50--65--75%.
  One to three signs will illuminate and blink quickly, indicating the chosen brightness level.
* 9 - Hold 3 seconds to reset system via WDT

## Power level configuration

The LED brightness (and power use) is controlled by the max PWM duty cycle we enable.

This can be changed in debug mode.

We persist this setting across reboots in the SmartEEPROM.

See https://community.atmel.com/forum/how-use-flash-atsamd51-j-20 for an example
of how to use the SmartEEPROM.
