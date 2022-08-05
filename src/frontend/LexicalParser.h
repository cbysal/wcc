#ifndef __LEXICAL_PARSER_H__
#define __LEXICAL_PARSER_H__

#include <string>
#include <utility>
#include <vector>

#include "Token.h"

class LexicalParser {
private:
  std::string fileName;
  bool isProcessed;
  unsigned long head;
  std::string content;
  std::vector<Token *> tokens;
  unsigned lineno;
  unsigned startLineno;
  unsigned stopLineno;

  void readFile();
  Token *nextToken();
  void parse();

public:
  LexicalParser(const std::string &);
  ~LexicalParser();

  std::string &getContent();
  std::pair<unsigned, unsigned> getLineno();
  std::vector<Token *> &getTokens();
};

#endif