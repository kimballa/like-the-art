// (c) Copyright 2022 Aaron Kimball

#ifndef _SENTENCE_H
#define _SENTENCE_H

class Sentence {
public:
  Sentence(unsigned int id, unsigned int signs): _id(id), _signs(signs) { };

  /** Return number of words in the sentence */
  unsigned int getNumWords() const { return __builtin_popcount(_signs); };

  unsigned int id() const { return _id; };

  /** Return the bit array representing the signs in the sentence. */
  unsigned int getSignBits() const { return _signs; };

  /** Light up the sentence (simple "appear" effect) */
  void enable();

  /** Turn off all words in the sentence. */
  void disable();

  /** Print this sentence to the serial console. */
  void toDbgPrint() const { logSentence(_signs); };

private:
  unsigned int _id;
  unsigned int _signs;
};

extern vector<Sentence> sentences;
extern "C" void setupSentences();

#endif /* _SENTENCE_H */
