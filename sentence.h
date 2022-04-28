// (c) Copyright 2022 Aaron Kimball

#ifndef _SENTENCE_H
#define _SENTENCE_H

class Sentence {
public:
  Sentence(unsigned int id, const vector<Sign*> &signs): _id(id), _signs(signs) { };

private:
  const unsigned int _id;
  vector<Sign*> _signs;
};

extern vector<Sentence> sentences;
extern "C" void initSentences();

#endif /* _SENTENCE_H */
