// (c) Copyright 2022 Aaron Kimball
//
// Sentence class definition and definition of all Sentence instances.

#include "like-the-art.h"

vector<Sentence> sentences;

// Helper function for initSentences()
static void _sentence(const vector<Sign*> &v) {
  static unsigned int nextSentenceId = 0;

  sentences.push_back(Sentence(nextSentenceId, v));
  nextSentenceId++;
}

void initSentences() {
  _sentence({ &S_YOU, &S_DONT, &S_HAVE, &S_TO, &S_LIKE, &S_ALL, &S_THE, &S_ART, &S_BANG });
  _sentence({ &S_DO, &S_YOU, &S_LIKE, &S_THE, &S_ART, &S_QUESTION });
  _sentence({ &S_DO, &S_YOU, &S_LIKE, &S_ART, &S_QUESTION });
  _sentence({ &S_LIKE, &S_THE, &S_ART, &S_BANG });
  _sentence({ &S_LOVE, &S_THE, &S_ART, &S_BANG });
  _sentence({ &S_HATE, &S_THE, &S_ART, &S_BANG });
  _sentence({ &S_WHY, &S_DO, &S_YOU, &S_LIKE, &S_ART, &S_QUESTION });
  _sentence({ &S_WHY, &S_DO, &S_YOU, &S_LOVE, &S_ART, &S_QUESTION });
  _sentence({ &S_WHY, &S_DO, &S_YOU, &S_HATE, &S_ART, &S_QUESTION });
  _sentence({ &S_WHY, &S_LIKE, &S_ALL, &S_ART, &S_QUESTION });
  _sentence({ &S_DO, &S_YOU, &S_LOVE, &S_QUESTION });
  _sentence({ &S_DO, &S_YOU, &S_HATE, &S_QUESTION });
  _sentence({ &S_WHY, &S_DO, &S_YOU, &S_LOVE, &S_QUESTION });
  _sentence({ &S_WHY, &S_DO, &S_YOU, &S_HATE, &S_QUESTION });
  _sentence({ &S_I, &S_LIKE, &S_ART, &S_BANG });
  _sentence({ &S_I, &S_LOVE, &S_ART, &S_BANG });
  _sentence({ &S_YOU, &S_DONT, &S_HAVE, &S_TO, &S_LIKE, &S_BM });

}
