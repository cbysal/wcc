#include <algorithm>
#include <iostream>
#include <regex>

#include "LexicalParser.h"

using namespace std;

#define BUF_SIZE 1024

const regex ID_REGEX("[A-Za-z_]\\w*");
const regex DEC_INT_REGEX("[1-9]\\d*");
const regex OCT_INT_REGEX("0[0-7]*");
const regex HEX_INT_REGEX("0[Xx][\\dA-Fa-f]+");
const regex FLOAT_REGEX(
    "((\\d+\\.(\\d+)?|\\.\\d+)([Ee][+-]?\\d+)?|\\d+([Ee][+-]?\\d+))|0[Xx](["
    "\\dA-Fa-f]+\\.?([\\dA-Fa-f]+)?|\\.[\\dA-Fa-f]+)[Pp][+-]?\\d+");

char buf[BUF_SIZE + 1];

LexicalParser::LexicalParser(string fileName) {
  this->fileName = fileName;
  this->isProcessed = false;
  this->head = 0;
  this->lineCount = 1;
  this->columnCount = 1;
}

LexicalParser::~LexicalParser() {
  for (Token *token : tokens)
    delete token;
  tokens.clear();
}

void LexicalParser::readFile() {
  FILE *file = fopen(fileName.data(), "r");
  if (file == nullptr) {
    cerr << "Fail to open file: " << fileName << endl;
    exit(-1);
  }
  unsigned n;
  while ((n = fread(buf, 1, BUF_SIZE, file)) != 0)
    content.append(buf, n);
  fclose(file);
}

void LexicalParser::parse() {
  if (isProcessed)
    return;
  isProcessed = true;
  readFile();
  Token *token;
  while ((token = nextToken()) != nullptr)
    tokens.push_back(token);
}

Token *LexicalParser::nextToken() {
  Token *token;
  if (head == content.length())
    return nullptr;
  while (head < content.length() && isspace(content[head]))
    head++;
  if (head == content.length())
    return nullptr;
  switch (content[head]) {
  case '!':
    if (head + 1 == content.length() || content[head + 1] != '=') {
      token = new Token(lineCount, columnCount, L_NOT);
      head++;
      return token;
    }
    token = new Token(lineCount, columnCount, NE);
    head += 2;
    return token;
  case '%':
    token = new Token(lineCount, columnCount, MOD);
    head++;
    return token;
  case '&':
    token = new Token(lineCount, columnCount, L_AND);
    head += 2;
    return token;
  case '(':
    token = new Token(lineCount, columnCount, LP);
    head++;
    return token;
  case ')':
    token = new Token(lineCount, columnCount, RP);
    head++;
    return token;
  case '*':
    token = new Token(lineCount, columnCount, MUL);
    head++;
    return token;
  case '+':
    token = new Token(lineCount, columnCount, PLUS);
    head++;
    return token;
  case ',':
    token = new Token(lineCount, columnCount, COMMA);
    head++;
    return token;
  case '-':
    token = new Token(lineCount, columnCount, MINUS);
    head++;
    return token;
  case '/':
    if (head + 1 == content.length()) {
      token = new Token(lineCount, columnCount, DIV);
      head++;
      return token;
    }
    switch (content[head + 1]) {
    case '*': {
      head = content.find("*/", head + 2) + 2;
      return nextToken();
    }
    case '/': {
      size_t index = content.find('\n', head + 2);
      if (index == string::npos)
        return nullptr;
      head = index + 1;
      return nextToken();
    }
    default:
      token = new Token(lineCount, columnCount, DIV);
      head++;
      return token;
    }
  case ';':
    token = new Token(lineCount, columnCount, SEMICOLON);
    head++;
    return token;
  case '<':
    if (head + 1 == content.length() || content[head + 1] != '=') {
      token = new Token(lineCount, columnCount, LT);
      head++;
      return token;
    }
    token = new Token(lineCount, columnCount, LE);
    head += 2;
    return token;
  case '=':
    if (head + 1 == content.length() || content[head + 1] != '=') {
      token = new Token(lineCount, columnCount, ASSIGN);
      head++;
      return token;
    }
    token = new Token(lineCount, columnCount, EQ);
    head += 2;
    return token;
  case '>':
    if (head + 1 == content.length() || content[head + 1] != '=') {
      token = new Token(lineCount, columnCount, GT);
      head++;
      return token;
    }
    token = new Token(lineCount, columnCount, GE);
    head += 2;
    return token;
  case '[':
    token = new Token(lineCount, columnCount, LB);
    head++;
    return token;
  case ']':
    token = new Token(lineCount, columnCount, RB);
    head++;
    return token;
  case '{':
    token = new Token(lineCount, columnCount, LC);
    head++;
    return token;
  case '|':
    token = new Token(lineCount, columnCount, L_OR);
    head += 2;
    return token;
  case '}':
    token = new Token(lineCount, columnCount, RC);
    head++;
    return token;
  default: {
    unsigned tail = head;
    while (tail < content.length() &&
           (isalnum(content[tail]) || content[tail] == '_'))
      tail++;
    string tokenStr = content.substr(head, tail - head);
    if (regex_match(tokenStr, ID_REGEX)) {
      if (KEYWORDS.find(tokenStr) != KEYWORDS.end()) {
        token = new Token(lineCount, columnCount, KEYWORDS.at(tokenStr));
        head = tail;
        return token;
      }
      token = new Token(lineCount, columnCount, ID, tokenStr);
      head = tail;
      return token;
    }
    tail = head;
    while (tail < content.length() &&
           (isxdigit(content[tail]) || content[tail] == '.' ||
            content[tail] == 'E' || content[tail] == 'e' ||
            content[tail] == 'P' || content[tail] == 'p' ||
            content[tail] == 'X' || content[tail] == 'x' ||
            content[tail] == '+' || content[tail] == '-'))
      tail++;
    tokenStr = content.substr(head, tail - head);
    if (regex_match(tokenStr, FLOAT_REGEX)) {
      token = new Token(lineCount, columnCount, FLOAT_LITERAL, stof(tokenStr));
      head = tail;
      return token;
    }
    tail = head;
    while (tail < content.length() &&
           (isxdigit(content[tail]) || content[tail] == 'X' ||
            content[tail] == 'x'))
      tail++;
    tokenStr = content.substr(head, tail - head);
    if (regex_match(tokenStr, HEX_INT_REGEX)) {
      token = new Token(lineCount, columnCount, INT_LITERAL,
                        (int)stol(tokenStr, 0, 16));
      head = tail;
      return token;
    }
    tail = head;
    while (tail < content.length() && isdigit(content[tail]))
      tail++;
    tokenStr = content.substr(head, tail - head);
    if (regex_match(tokenStr, DEC_INT_REGEX)) {
      token =
          new Token(lineCount, columnCount, INT_LITERAL, (int)stol(tokenStr));
      head = tail;
      return token;
    }
    tail = head;
    while (tail < content.length() &&
           (content[tail] >= '0' && content[tail] <= '7'))
      tail++;
    tokenStr = content.substr(head, tail - head);
    if (regex_match(tokenStr, OCT_INT_REGEX)) {
      token = new Token(lineCount, columnCount, INT_LITERAL,
                        (int)stol(tokenStr, 0, 8));
      head = tail;
      return token;
    }
  }
  }
  return nullptr;
}

string LexicalParser::getContent() { return content; }

vector<Token *> &LexicalParser::getTokens() {
  if (!isProcessed)
    parse();
  return tokens;
}