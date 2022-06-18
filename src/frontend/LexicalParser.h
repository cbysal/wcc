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
  unsigned long head;
  string content;
  vector<Token *> tokens;
  unsigned lineno;
  unsigned startLineno;
  unsigned stopLineno;

  void readFile();
  Token *nextToken();
  void parse();

public:
  LexicalParser(const string &);
  ~LexicalParser();

  string &getContent();
  pair<unsigned, unsigned> getLineno();
  vector<Token *> &getTokens();
};

#endif