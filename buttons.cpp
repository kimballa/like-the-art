// (c) Copyright 2022 Aaron Kimball

#include "like-the-art.h"

static constexpr uint8_t BTN0_PIN = 11; // Button 0 is on D11.

// PCF8574N on channel 0x23 reads buttons 1--8.
static I2CParallel buttonBank;

// Keep a rolling history buffer of button presses
static constexpr uint8_t CODE_LENGTH = 10;
static constexpr uint8_t MAX_PRESS_HISTORY = CODE_LENGTH + 1;
static const uint8_t adminCodeSequence[CODE_LENGTH] = { 1, 0, 4, 8, 5, 1, 5, 6, 6, 3 };
static uint8_t buttonHistory[MAX_PRESS_HISTORY]; // Circular rolling history buffer
static uint8_t firstPressIdx;
static uint8_t nextPressIdx;


// In the RUNNING state, every 'N' button presses... we randomize what all the buttons do.
static constexpr uint8_t BUTTON_ROTATION_THRESHOLD = 25;
static uint8_t numButtonPresses = 0;

// All 9 Button instances.
vector<Button> buttons;

static void wipePasswordHistory() {
  firstPressIdx = 0;
  nextPressIdx = 0;
  for (uint8_t i = 0; i < MAX_PRESS_HISTORY; i++) {
    buttonHistory[i] = 0;
  }
}

/** Initial setup of buttons invoked by the setup() method. */
void setupButtons() {
  buttonBank.init(3 + I2C_PCF8574_MIN_ADDR, I2C_SPEED_STANDARD);
  buttonBank.enableInputs(0xFF); // all 8 channels of button bank are inputs.

  pinMode(BTN0_PIN, INPUT_PULLUP);

  // Allocate the button state and dispatch handlers.
  buttons.clear();
  buttons.reserve(NUM_BUTTONS);
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    buttons.emplace_back(i, defaultBtnHandler);
  }

  wipePasswordHistory();
  attachStandardButtonHandlers();
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

  DBGPRINTU("Registered keypress:", btnId);

  if (macroState == MS_ADMIN) {
    // Don't track the rolling history when we're already in admin mode;
    // we don't want to accidentally reset to the beginning of the admin mode
    // state machine if we're poking things deeper into the admin state machine
    // in exactly the wrong way.
    return;
  }

  // Record the latest button press
  buttonHistory[nextPressIdx] = btnId;
  nextPressIdx = (nextPressIdx + 1) % MAX_PRESS_HISTORY;
  if (nextPressIdx == firstPressIdx) {
    firstPressIdx = (firstPressIdx + 1) % MAX_PRESS_HISTORY;
  }

  // Check whether last N button presses matched the admin code.
  bool match = true;
  unsigned int counted = 0;
  for (uint8_t i = 0; i < min(CODE_LENGTH, (uint8_t)(nextPressIdx - firstPressIdx)); i++) {
    counted++;
    if (buttonHistory[(i + firstPressIdx) % MAX_PRESS_HISTORY] != adminCodeSequence[i]) {
      match = false;
    }
    if (!match) {
      break; // No need to continue comparison.
    }
  }

  if (match && counted == CODE_LENGTH) {
    // The user has keyed in the admin access code sequence.
    // Switch to admin macro state.
    setMacroStateAdmin();
    wipePasswordHistory();
  } else {
    // Increment the number of buttons that we've seen pressed.
    numButtonPresses++;

    // If that reaches the scrambling threshold, mix up the assignments for
    // all the button handlers and reset the numButtonPresses counter.
    if (numButtonPresses >= BUTTON_ROTATION_THRESHOLD) {
      attachStandardButtonHandlers();
    }
  }
}

/** A button handler that does nothing; for states when a given button is unmapped. */
void emptyBtnHandler(uint8_t btnId, uint8_t btnState) {
}

/** Default button-press handler that just records history. */
void defaultBtnHandler(uint8_t btnId, uint8_t btnState) {
  recordButtonHistory(btnId, btnState);
}


Button::Button(uint8_t id, buttonHandler_t handlerFn):
    _id(id), _curState(BTN_OPEN), _priorPoll(BTN_OPEN), _readStartTime(0),
    _pushDebounceInterval(BTN_DEBOUNCE_MILLIS),
    _releaseDebounceInterval(BTN_DEBOUNCE_MILLIS),
    _handlerFn(handlerFn) {

  if (NULL == _handlerFn) {
    _handlerFn = defaultBtnHandler;
  }
}

bool Button::update(uint8_t latestPoll) {
  latestPoll = (latestPoll != 0); // Collapse input into a 1/0 universe.

  if (latestPoll != _priorPoll) {
    // Input has changed since we last polled. Reset debounce timer.
    _readStartTime = millis();
  }

  // Save reading for next interrogation of update().
  _priorPoll = latestPoll;

  // Decide which debounce interval to use, depending on whether we're monitoring
  // for a next state change of "push" (0 to 1) or "release" (1 to 0).
  unsigned int debounceInterval = _pushDebounceInterval;
  if (_curState == BTN_PRESSED) {
    debounceInterval = _releaseDebounceInterval;
  }

  if ((millis() - _readStartTime) > debounceInterval) {
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

//// Button handler functions that change the active sentence or the active effect ////

// Several buttons fix a particular active animation effect for several seconds:
#define EFFECT_BTN_NAME(effect_enum) btnHandlerEffect_##effect_enum
#define EFFECT_BTN_FN(effect_enum) \
  extern "C" void EFFECT_BTN_NAME(effect_enum) (uint8_t btnId, uint8_t btnState) { \
    recordButtonHistory(btnId, btnState); \
    lockEffect(effect_enum); \
  }

EFFECT_BTN_FN(EF_APPEAR);
EFFECT_BTN_FN(EF_GLOW);
EFFECT_BTN_FN(EF_BLINK);
EFFECT_BTN_FN(EF_BLINK_FAST);
EFFECT_BTN_FN(EF_ONE_AT_A_TIME);
EFFECT_BTN_FN(EF_BUILD);
EFFECT_BTN_FN(EF_SNAKE);
EFFECT_BTN_FN(EF_SLIDE_TO_END);
EFFECT_BTN_FN(EF_MELT);

// Other buttons fix a particular sentence to be the active sentence for several seconds.
// There are 18 sentences defined, and we have a separate handler method to invoke each.
#define SENTENCE_BTN_NAME(sentence_id) btnHandlerSentence_##sentence_id
#define SENTENCE_BTN_FN(sentence_id) \
  extern "C" void SENTENCE_BTN_NAME(sentence_id) (uint8_t btnId, uint8_t btnState) { \
    recordButtonHistory(btnId, btnState); \
    lockSentence(sentence_id); \
  }

SENTENCE_BTN_FN(0);
SENTENCE_BTN_FN(1);
SENTENCE_BTN_FN(2);
SENTENCE_BTN_FN(3);
SENTENCE_BTN_FN(4);
SENTENCE_BTN_FN(5);
SENTENCE_BTN_FN(6);
SENTENCE_BTN_FN(7);
SENTENCE_BTN_FN(8);
SENTENCE_BTN_FN(9);
SENTENCE_BTN_FN(10);
SENTENCE_BTN_FN(11);
SENTENCE_BTN_FN(12);
SENTENCE_BTN_FN(13);
SENTENCE_BTN_FN(14);
SENTENCE_BTN_FN(15);
SENTENCE_BTN_FN(16);
SENTENCE_BTN_FN(17);

// All the handlers that can be assigned to the 9 buttons in the running MacroState.
static const buttonHandler_t userButtonFns[] = {
  EFFECT_BTN_NAME(EF_APPEAR),
  EFFECT_BTN_NAME(EF_GLOW),
  EFFECT_BTN_NAME(EF_BLINK),
  EFFECT_BTN_NAME(EF_BLINK_FAST),
  EFFECT_BTN_NAME(EF_ONE_AT_A_TIME),
  EFFECT_BTN_NAME(EF_BUILD),
  EFFECT_BTN_NAME(EF_SNAKE),
  EFFECT_BTN_NAME(EF_SLIDE_TO_END),
  EFFECT_BTN_NAME(EF_MELT),

  SENTENCE_BTN_NAME(0),
  SENTENCE_BTN_NAME(1),
  SENTENCE_BTN_NAME(2),
  SENTENCE_BTN_NAME(3),
  SENTENCE_BTN_NAME(4),
  SENTENCE_BTN_NAME(5),
  SENTENCE_BTN_NAME(6),
  SENTENCE_BTN_NAME(7),
  SENTENCE_BTN_NAME(8),
  SENTENCE_BTN_NAME(9),
  SENTENCE_BTN_NAME(10),
  SENTENCE_BTN_NAME(11),
  SENTENCE_BTN_NAME(12),
  SENTENCE_BTN_NAME(13),
  SENTENCE_BTN_NAME(14),
  SENTENCE_BTN_NAME(15),
  SENTENCE_BTN_NAME(16),
  SENTENCE_BTN_NAME(17),
};

// The length of the userButtonFns array.
static constexpr unsigned int NUM_USER_BUTTON_FNS = std::size(userButtonFns);
unsigned int numUserButtonFns() {
  return NUM_USER_BUTTON_FNS;
}

// The same array as userButtonFns, but with elements in shuffled order
// to enable random button assignments.
static buttonHandler_t shuffledUserButtonFns[NUM_USER_BUTTON_FNS];

/** Shuffles the order of button handler functions in shuffledUserButtonFns. */
static void shuffleButtonHandlers() {
  // Number of shuffle operations to perform.
  static constexpr unsigned int NUM_SHUFFLES = NUM_USER_BUTTON_FNS * 20;

  // Initialize the shuffled button handler array from the pristine handler array.
  memcpy(shuffledUserButtonFns, userButtonFns, sizeof(buttonHandler_t) * NUM_USER_BUTTON_FNS);

  // Perform a number of exchanges between random positions in the array.
  for (unsigned int i = 0; i < NUM_SHUFFLES; i++) {
    unsigned int idxA = random(NUM_USER_BUTTON_FNS);
    unsigned int idxB = random(NUM_USER_BUTTON_FNS);

    buttonHandler_t tmp = shuffledUserButtonFns[idxA];
    shuffledUserButtonFns[idxA] = shuffledUserButtonFns[idxB];
    shuffledUserButtonFns[idxB] = tmp;
  }
}


/** Attach button handlers for RUNNING mode -- assign random effects to each btn. */
void attachStandardButtonHandlers() {
  DBGPRINT("Setting randomly-assigned button handlers...");

  // Create an array of all possible button handlers in a random order...
  shuffleButtonHandlers();

  // ... And choose the first `NUM_BUTTONS` elements from it.
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    buttons[i].setHandler(shuffledUserButtonFns[i]);

    buttons[i].setPushDebounceInterval(BTN_DEBOUNCE_MILLIS);
    buttons[i].setReleaseDebounceInterval(BTN_DEBOUNCE_MILLIS);
  }

  numButtonPresses = 0; // Reset the counter for when to next scramble the buttons.
}

// For WAITING MacroState, attach button handlers that track history and can shift
// into admin mode but do not start or change any animations.
void attachWaitModeButtonHandlers() {
  DBGPRINT("Resetting to default button handlers...");

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    buttons[i].setHandler(defaultBtnHandler);
    buttons[i].setPushDebounceInterval(BTN_DEBOUNCE_MILLIS);
    buttons[i].setReleaseDebounceInterval(BTN_DEBOUNCE_MILLIS);
  }
}

/**
 * Attach the empty handler (and default timing) to all buttons.
 */
void attachEmptyButtonHandlers() {
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    buttons[i].setHandler(emptyBtnHandler);
    buttons[i].setPushDebounceInterval(BTN_DEBOUNCE_MILLIS);
    buttons[i].setReleaseDebounceInterval(BTN_DEBOUNCE_MILLIS);
  }
}
