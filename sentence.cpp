// (c) Copyright 2022 Aaron Kimball
//
// Sentence class definition and definition of all Sentence instances.

#include "like-the-art.h"

vector<Sentence> sentences;

// Helper function for setupSentences()
static unsigned int _sentence(const unsigned int signVector) {
  static unsigned int nextSentenceId = 0;

  sentences.emplace_back(nextSentenceId, signVector);
  nextSentenceId++;
}

// sentence id for "You don't have to like all the art!"
static unsigned int MAIN_MSG_SENTENCE_ID = 0;
unsigned int mainMsgId() {
  return MAIN_MSG_SENTENCE_ID;
}

void setupSentences() {
  MAIN_MSG_SENTENCE_ID =
      _sentence( S_YOU | S_DONT | S_HAVE | S_TO | S_LIKE | S_ALL | S_THE | S_ART | S_BANG );

  _sentence( S_DO | S_YOU | S_LIKE | S_THE | S_ART | S_QUESTION );
  _sentence( S_DO | S_YOU | S_LIKE | S_ART | S_QUESTION );
  _sentence( S_LIKE | S_THE | S_ART | S_BANG );
  _sentence( S_LOVE | S_THE | S_ART | S_BANG );
  _sentence( S_HATE | S_THE | S_ART | S_BANG );
  _sentence( S_WHY | S_DO | S_YOU | S_LIKE | S_ART | S_QUESTION );
  _sentence( S_WHY | S_DO | S_YOU | S_LOVE | S_ART | S_QUESTION );
  _sentence( S_WHY | S_DO | S_YOU | S_HATE | S_ART | S_QUESTION );
  _sentence( S_WHY | S_LIKE | S_ALL | S_ART | S_QUESTION );
  _sentence( S_DO | S_YOU | S_LOVE | S_QUESTION );
  _sentence( S_DO | S_YOU | S_HATE | S_QUESTION );
  _sentence( S_WHY | S_DO | S_YOU | S_LOVE | S_QUESTION );
  _sentence( S_WHY | S_DO | S_YOU | S_HATE | S_QUESTION );
  _sentence( S_I | S_LIKE | S_ART | S_BANG );
  _sentence( S_I | S_LOVE | S_ART | S_BANG );
  _sentence( S_YOU | S_DONT | S_HAVE | S_TO | S_LIKE | S_BM | S_BANG );
  _sentence( S_WHY | S_DO | S_YOU | S_LOVE | S_BM | S_QUESTION );
}

/** Turn on the signs for sentence */
void Sentence::enable() {
  for (unsigned int i = 0; i < NUM_SIGNS; i++) {
    if (_signs & (1 << i)) {
      signs[i].enable();
    }
  }
}

/** Turn on the signs for sentence (and only those signs) */
void Sentence::enableExclusively() {
  for (unsigned int i = 0; i < NUM_SIGNS; i++) {
    if (_signs & (1 << i)) {
      signs[i].enable();
    } else {
      signs[i].disable(); // Not part of this sentence.
    }
  }
}

/** Turn signs participating in this sentence off. */
void Sentence::disable() {
  for (unsigned int i = 0; i < NUM_SIGNS; i++) {
    if (_signs & (1 << i)) {
      signs[i].disable(); // It's an element of this sentence; turn it off.
    }
  }
}

