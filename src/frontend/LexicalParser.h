#ifndef __LEXICAL_PARSER_H__
#define __LEXICAL_PARSER_H__

#include <string>
#include <unordered_map>
#include <vector>

#include "Token.h"

using namespace std;

const unordered_map<string, Type> KEYWORDS = {
    {"break", BREAK}, {"const", CONST},   {"continue", CONTINUE},
    {"else", ELSE},   {"float", FLOAT},   {"if", IF},
    {"int", INT},     {"return", RETURN}, {"void", VOID},
    {"while", WHILE}};

class LexicalParser {
private:
  string fileName;
  bool isProcessed;
  unsigned head;
  unsigned lineCount;
  unsigned columnCount;
  string content;
  vector<Token *> tokens;

  void readFile();
  Token *nextToken();
  void parse();

public:
  LexicalParser(string);
  ~LexicalParser();

  string getContent();
  vector<Token *> &getTokens();
};

#endif