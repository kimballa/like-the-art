// (c) Copyright 2022 Aaron Kimball

#include "like-the-art.h"

static constexpr uint8_t BTN0_PIN = 11; // Button 0 is on D11.

// PCF8574N on channel 0x22 reads buttons 1--8.
static I2CParallel buttonBank;

// Keep a rolling history buffer of button presses
static vector<uint8_t> buttonPresses;
static vector<uint8_t> adminCodeSequence = { 1, 0, 4, 8, 5, 1, 5, 6, 6, 3 };

// All 9 Button instances.
static vector<Button> buttons;

/** Initial setup of buttons invoked by the setup() method. */
void setupButtons() {
  buttonBank.init(   2 + I2C_PCF8574_MIN_ADDR, I2C_SPEED_STANDARD);
  buttonBank.enableInputs(0xFF); // all 8 channels of button bank are inputs.

  pinMode(BTN0_PIN, INPUT_PULLUP);

  // Allocate the button state and dispatch handlers.
  buttons.clear();
  buttons.reserve(NUM_BUTTONS);
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    buttons.emplace_back(i, defaultBtnHandler);
  }

  // TODO(aaron): Attach more interesting button handlers to each Button.
}

/**
 * Poll the I/O channels for the buttons and check whether any have been pressed.
 *
 * All buttons are normally-open and signal lines are connected to pull-up resistors.
 * Button press will pull the signal to GND as seen by the Arduino or PCF8574. One
 * button [#0] is directly connected to the Arduino, buttons 1..8 thru PCF8574.
 *
 * (Technically, the buttons have pull-down resistors and switch to 3V3 when pressed;
 * but they then pass through a 74LVC14A Schmitt-trigger inverter before passing
 * onto the communications net described above.)
 */
void pollButtons() {
  // Button 0 is connected directly to a digital gpio input
  buttons[0].update(digitalRead(BTN0_PIN));

  // Buttons 1..8 are connected thru the PCF8574 and are read as a byte.
  uint8_t btnBankState = buttonBank.read();
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t btnState = (btnBankState >> i) & 0x1;
    buttons[i + 1].update(btnState);
  }
}

/**
 * Record a rolling history of the most recent button presses.
 * If the user has entered the sequence that enables admin mode, switch to that
 * macro state.
 */
static void recordButtonHistory(uint8_t btnId, uint8_t btnState=BTN_PRESSED) {
  if (btnState != BTN_PRESSED) {
    return; // Button was released; don't record.
  }

  DBGPRINT("Registered keypress: " + to_string(btnId));

  if (macroState == MS_ADMIN) {
    // Don't track the rolling history when we're already in admin mode;
    // we don't want to accidentally reset to the beginning of the admin mode
    // state machine if we're poking things deeper into the admin state machine
    // in exactly the wrong way.
    return;
  }

  // Record the latest button press
  buttonPresses.push_back(btnId);
  // But don't track a history longer than the admin access code sequence.
  int deleteCount = buttonPresses.size() - adminCodeSequence.size();
  if (deleteCount > 0) {
    buttonPresses.erase(buttonPresses.begin(), buttonPresses.begin() + deleteCount);
  }

  if (buttonPresses == adminCodeSequence) {
    // The user has keyed in the admin access code sequence.
    // Switch to admin macro state.
    macroState = MS_ADMIN;
    adminState = AS_INITIALIZING;
    buttonPresses.clear();
    DBGPRINT(">>>> Entering ADMIN MacroState <<<<");
  }
}

/** Default button-press handler that just records history. */
void defaultBtnHandler(uint8_t btnId, uint8_t btnState) {
  recordButtonHistory(btnId, btnState);
}


Button::Button(uint8_t id, buttonHandler_t handlerFn):
    _id(id), _curState(BTN_OPEN), _priorPoll(BTN_OPEN), _readStartTime(0),
    _handlerFn(handlerFn) {

  if (NULL == _handlerFn) {
    _handlerFn = defaultBtnHandler;
  }
}

bool Button::update(uint8_t latestPoll) {
  latestPoll = (latestPoll != 0); // Collapse input into a 1/0 universe.

  if (latestPoll != _priorPoll) {
    // Input has changed since we last polled. Reset debounce timer.
    _readStartTime = micros();
  }

  // Save reading for next interrogation of update().
  _priorPoll = latestPoll;

  if ((micros() - _readStartTime) > BTN_DEBOUNCE_MICROS) {
    // The reading has remained consistent for the debounce interval.
    // It's a legitimate state.

    if (latestPoll != _curState) {
      _curState = latestPoll; // Lock this in as the new state.
      (*_handlerFn)(_id, _curState); // Invoke callback handler.
      return true;
    }
  }

  return false; // No state change.
}


