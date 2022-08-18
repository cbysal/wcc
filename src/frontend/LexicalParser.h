#ifndef __LEXICAL_PARSER_H__
#define __LEXICAL_PARSER_H__

#include <string>
#include <utility>
#include <vector>

#include "Token.h"

class LexicalParser {
private:
  std::string fileName;
  unsigned long head;
  std::string content;
  unsigned lineno;

  void readFile();
  Token *nextToken();

public:
  LexicalParser();

  void setInput(const std::string &);
  void parse();
};

#endif