// (c) Copyright 2022 Aaron Kimball

#ifndef _SENTENCE_H
#define _SENTENCE_H

class Sentence {
public:
  Sentence(unsigned int id, unsigned int signs): _id(id), _signs(signs) { };

  void enable();
  void disable();

private:
  const unsigned int _id;
  const unsigned int _signs;
};

extern vector<Sentence> sentences;
extern "C" void setupSentences();

#endif /* _SENTENCE_H */
