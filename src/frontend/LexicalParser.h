#ifndef __LEXICAL_PARSER_H__
#define __LEXICAL_PARSER_H__

#include <string>
#include <unordered_map>
#include <vector>

#include "Token.h"

class LexicalParser {
private:
  string fileName;
  bool isProcessed;
  unsigned head;
  string content;
  vector<Token *> tokens;

  void readFile();
  Token *nextToken();
  void parse();

public:
  LexicalParser(const string &);
  ~LexicalParser();

  string &getContent();
  vector<Token *> &getTokens();
};

#endif