// (C) Copyright 2022 Aaron Kimball
//
// Sign class and SignChannel class implementations.
// Sign channels work on INVERTED logic -- we pull a pin high to disable a sign, low to turn
// it on.

#include "like-the-art.h"

void I2CSignChannel::setup() {
  disable();
}

void I2CSignChannel::enable() {
  _bus.setOr(_bit); // Set our pin bit high.
}

void I2CSignChannel::disable() {
  _bus.setAnd(~_bit); // Set our pin bit low.
}


void GpioSignChannel::setup() {
  pinMode(_pin, OUTPUT);
  disable();
}

void GpioSignChannel::enable() {
  digitalWrite(_pin, 1);
}

void GpioSignChannel::disable() {
  digitalWrite(_pin, 0);
}


void _Sign::setup(unsigned int id, const string &word, SignChannel *channel) {
  this->id = id;
  this->word = word;
  this->channel = channel;

  channel->setup();
}

void _Sign::enable() {
  DBGPRINT("ON (" + to_string(id) + "): " + word);
  this->channel->enable();
}

void _Sign::disable() {
  DBGPRINT("OFF (" + to_string(id) + "): " + word);
  this->channel->disable();
}

_Sign::~_Sign() {
  if (NULL != channel) {
    delete channel;
    channel = NULL;
  }
}

Sign S_WHY;
Sign S_DO;
Sign S_YOU;
Sign S_DONT;
Sign S_HAVE;
Sign S_TO;
Sign S_I;
Sign S_LIKE;
Sign S_LOVE;
Sign S_HATE;
Sign S_BM;
Sign S_ALL;
Sign S_THE;
Sign S_ART;
Sign S_BANG;
Sign S_QUESTION;

vector<Sign*> signs;

void setupSigns(I2CParallel &bank0, I2CParallel &bank1) {
  // Define Signs with bindings to I/O channels.
  S_WHY.setup(0, "WHY", new I2CSC(bank0, 0));
  S_DO.setup(1, "DO", new NSC());
  S_YOU.setup(2, "YOU", new NSC());
  S_DONT.setup(3, "DON'T", new NSC());
  S_HAVE.setup(4, "HAVE", new NSC());
  S_TO.setup(5, "TO", new NSC());
  S_I.setup(6, "I", new NSC());
  S_LIKE.setup(7, "LIKE", new NSC());
  S_LOVE.setup(8, "LOVE", new NSC());
  S_HATE.setup(9, "HATE", new NSC());
  S_BM.setup(10, ")'(", new NSC());
  S_ALL.setup(11, "ALL", new NSC());
  S_THE.setup(12, "THE", new NSC());
  S_ART.setup(13, "ART", new NSC());
  S_BANG.setup(14, "!", new NSC());
  S_QUESTION.setup(15, "?", new NSC());

  // Load them all into an array for iteration purposes.
  signs.clear();
  signs.push_back(&S_WHY);
  signs.push_back(&S_DO);
  signs.push_back(&S_YOU);
  signs.push_back(&S_DONT);
  signs.push_back(&S_HAVE);
  signs.push_back(&S_TO);
  signs.push_back(&S_I);
  signs.push_back(&S_LIKE);
  signs.push_back(&S_LOVE);
  signs.push_back(&S_HATE);
  signs.push_back(&S_BM);
  signs.push_back(&S_ALL);
  signs.push_back(&S_THE);
  signs.push_back(&S_ART);
  signs.push_back(&S_BANG);
  signs.push_back(&S_QUESTION);
}

