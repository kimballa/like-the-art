// (c) Copyright 2022 Aaron Kimball

#ifndef _BUTTONS_H_
#define _BUTTONS_H_

/* 25 ms delay for a button press to be registered as "valid." */
constexpr unsigned int BTN_DEBOUNCE_MILLIS = 25;

constexpr uint8_t NUM_BUTTONS = 9;

// Button::getState() returns 0 if a button is registered as pressed, 1 if open.
constexpr uint8_t BTN_PRESSED = 0;
constexpr uint8_t BTN_OPEN = 1;

// A function called whenever a button has definitively changed state.
typedef void (*buttonHandler_t)(uint8_t id, uint8_t btnState);

class Button {
public:
  Button(uint8_t id, buttonHandler_t handlerFn);

  /**
   * Called to tell the button about the latest polled signal and register
   * as a press/release if appropriate.
   *
   * Returns true if the state has decisively changed, false otherwise.
   */
  bool update(uint8_t latestPoll);

  /** Returns 0 if button pressed, 1 if open. */
  uint8_t getState() const { return _curState; };

  void setHandler(buttonHandler_t handlerFn) { _handlerFn = handlerFn; };
  unsigned int getDebounceInterval() const { return _debounceInterval; };
  void setDebounceInterval(unsigned int debounce) { _debounceInterval = debounce; };

private:
  uint8_t _id;
  uint8_t _curState;
  uint8_t _priorPoll;
  uint32_t _readStartTime;
  unsigned int _debounceInterval;
  buttonHandler_t _handlerFn;
};

extern "C" {
  void setupButtons();
  void pollButtons();
  void defaultBtnHandler(uint8_t btnId, uint8_t btnState);
  void emptyBtnHandler(uint8_t btnId, uint8_t btnState);
  void attachStandardButtonHandlers();
  void attachEmptyButtonHandlers();
};

extern vector<Button> buttons;

#endif /* _BUTTONS_H_ */
