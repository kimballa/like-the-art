
# You Don't Have To Like All The Art!

An art project, built on Arduino.

This sketch manages power distribution to a collection of LED signs that spell sentences
when lit, and allows the viewer some semblance of control through a collection of buttons
on a controller box.

## LED Neon Power

Power is distributed through a collection of power MOSFETs. We can activate them through
our I/O which is a combination of GPIO and 8-bit parallel busses (PCF8574) on I2C. Control
signals are NAND'd together with a PWM signal we drive for global brightness control and
control of certain effects; the output of the PWM signal NAND'd with each control line is
fed into a 75374 low-side driver that converts a logic-level signal into a high-voltage
gate signal for the power MOSFETs. The '374 is negative logic; we drive that input low to
illuminate the LEDs. However, as we have a NAND layer in front of it, these cancel out so
a control signal should be high to light an LED sign and that sign is lit only when the
PWM duty cycle is also high.

## Ambient operation

By default, the sign will randomly select messages from a catalog of preprogrammed
sentences, and display each with a randomly-chosen effect from a catalog of effects. Zero
or more randomly selected overlay effects (e.g. "glitching") may also be employed in
conjunction with the main effect animation.

## Daylight sensor

The system has a high-level state machine that selects between one of three different
"macro states": `RUNNING`, `WAITING`, or `ADMIN`. The running state is described above in
the "Ambient operation" section. Admin mode is a specific menu for administrative and
debugging control, described further on.

The system is only `RUNNING` when it is dark outside. A photosensor tuned to UV light
(940nm) is connected to the base of a common-emitter NPN transistor whose output is read
by the Arduino as an analog input. When the photosensor is not receiving much light,
the input signal (named `DARK`) is high, and the system is placed in the `RUNNING` macro
state and uses its main ambient state machine.

If `DARK` is low, the system is placed in the `WAITING` macro state, which remains
principally idle, just monitoring the photosensor and button inputs for the admin
passcode.

The sensor is Schmitt triggered so as to avoid hysteresis around twilight. Once the macro
state has changed in either direction, it remains locked for at least one minute. There is
approximately a 10% gap between the going-`DARK` and going-bright thresholds for the
analog sensor; by default, 640/1024 triggers `DARK` (`RUNNING`) and 580/1024 triggers "daylight"
(`WAITING`) mode. This can be adjusted with one of the controls in admin mode.

Because the sensor is specific to the 940nm band, high output from the LEDs (around 680nm)
does not falsely cause the sensor to believe it is daylight.

## Button inputs

A 3x3 button grid offers user input.

* Some buttons change the active message
* Other buttons change the active effect

When a user presses a button that selects a message, that message will be displayed for
the next 20 seconds of animations, and then go back to the carousel.

When a user presses a button that selects an active effect, that effect will persist on
all messages for the next 20 seconds before reverting to random.

Every so often, a button press may scramble which buttons map to which operation
(message select or effect select).

Don't press too many buttons too quickly, or else the system will get mad at you and
throw a tantrum.

Buttons do not cause any visible effect when in `WAITING` mode.

## Admin mode

The buttons form a grid much like a telephone number pad:

```
1 2 3
4 5 6
7 8 9
```

Recent button inputs are recorded in a shift register. When an access code is entered, the
device switches into admin mode. If you make a typo, just start again from the beginning.
(While in the `WAITING` state, buttons do not cause visible effect changes to the main
display, but can still be used for access code entry. Likewise, when in "tantrum mode" and
not responding to inputs, you can continue to enter an access code.)

The first LED sign will begin blinking slowly when admin mode is active, indicating you
are at the main menu.

In this mode, the buttons choose between the following menu of options:

* 1 - Full sign test. Automatically light up each sign in series for one second each.
  Repeats indefinitely until a new operation is chosen.
* 2 - Light up one sign at a time without automatically progressing. Use buttons 4 and 6
  to move the "cursor" back/forward. Press button 9 to return to the top level menu. (First
  sign blinks slowly again when at main menu.)
* 3 - Change the active effect. A sample message is lit while you choose the effect. Use 4
  and 6 to scroll back/forward through the elements of the `Effect` enumeration. 9 returns
  to the top menu. The sample message can be changed with option `4` at the main menu.
* 4 - Light up sentences one-by-one; don't auto-progress. 4/6/9 as before.
  This uses the active effect chosen with option `3` from the main menu.
* 5 - Choose the brightness level. Use buttons 1--4 where 1 is super-low, 2 is low, 3 is medium-high,
  and 4 is high-intensity brightness (50, 60, 75, 100% full power respectively).
  One to four signs will illuminate and blink quickly, indicating the chosen brightness level.
  9 to return to top menu.
* 6 - Light up all signs at the configured brightness level.
* 7 - Hold for 1 second then release to exit admin mode. The first 3 signs flash 3 times and then
  admin mode ends. State is always reset to `RUNNING` on exit; if it's daylight, the sensor will
  then put the system into the `WAITING` state within one minute.
* 8 - Calibrate the daylight sensor. In "neutral" calibration, the 6th LED sign will blink
  slowly. You can use the `4` and `6` buttons to shift the sensor threshold down (brighter
  light still counts as `DARK`) or up (must be even darker outside to trigger `DARK`)
  respectively. Default calibration is 640/1024 for dark, 580/1024 for light; the buttons
  move these thresholds up or down in 20/1024 increments up to +5 (740 and 680) or down to
  -5 (540 and 480). Up to five LEDs to the left or right of the 6th ("center") LED will
  blink to indicate the calibration level up to -5 or +5 clicks of 20/1024 each. The
  *last* LED on the sign will be lit if the sensor thinks it's DARK outside. The
  calibration level will persist across reboots.
* 9 - Hold for 3 seconds then release to reboot the system. First 3 signs flash quickly
  five times and then the system is reset. The programmed LED brightness level and daylight
  sensor calibration will persist across system reboot or power-down events.

## Power level configuration

The LED brightness (and power use) is controlled by the max PWM duty cycle we enable.

This can be changed in admin mode.

We persist this setting across reboots in the SmartEEPROM. See `lib/smarteeprom.cpp` for
low-level implementation; `saveconfig.cpp` for application-specific layer.

