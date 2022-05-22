
# You Don't Have To Like All The Art!

An art project, built on Arduino.

This sketch manages power distribution to a collection of LED signs that spell
sentences when lit, and allows the viewer some semblance of control through
a collection of buttons on a controller box.

## LED Neon Power

Power is distributed through a collection of power MOSFETs. We
can activate them through our I/O which is a combination of GPIO and 8-bit
parallel busses (PCF8574) on I2C. Control signals are NAND'd together with a
PWM signal we also control for brightness and effects control; that is fed into
a 75374 low-side driver that converts a logic-level signal into a high-voltage gate
signal for the power MOSFETs. The '374 is negative logic; we drive that input
low to illuminate the LEDs. However, as we have a NAND layer in front of it, these
cancel out so a control signal should be high to light an LED sign and that sign is lit
only when the PWM duty cycle is also high.

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

## Admin mode

The buttons form a grid much like a telephone number pad:

```
1 2 3
4 5 6
7 8 9
```

Recent button inputs are recorded in a shift register. When an access code is entered, the
device switches into admin mode.

The first LED sign will begin blinking when admin mode is active.

In this mode, the buttons choose between the following menu of options:

* 1 - Full sign test. Auto Light up each sign in series. Repeats indefinitely until a new
  operation is chosen.
* 2 - Light up signs one-by-one; don't auto-progress. Use buttons 4 and 6 to go back/forward. Use
  button 9 to return to the top level menu.
* 3 - Change active effect. A sample message is lit while you choose the effect. Use 4 and 6 to
  scroll back/forward. 9 returns to the top menu.
* 4 - Light up sentences one-by-one; don't auto-progress. 4/6/9 as before.
* 5 - Choose the brightness level. Use buttons 1--4 where 1 is super-low, 2 is low, 3 is standard,
  and 4 is high-intensity brightness (50, 60, 75, 100% full power respectively).
  One to four signs will illuminate and blink quickly, indicating the chosen brightness level.
  9 to return to top menu.
* 6 - Light up all signs at configured brightness level.
* 7 - Hold 1 second to exit admin mode. The first 3 signs flash 3 times and then
  admin mode ends.
* 8 - No function.
* 9 - Hold 3 seconds to completely reset system.

## Power level configuration

The LED brightness (and power use) is controlled by the max PWM duty cycle we enable.

This can be changed in admin mode.

We persist this setting across reboots in the SmartEEPROM. See `lib/smarteeprom.cpp` for
low-level implementation; `saveconfig.cpp` for application-specific layer.

