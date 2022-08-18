#include <algorithm>
#include <iostream>
#include <regex>
#include <unordered_map>

#include "../GlobalData.h"
#include "LexicalParser.h"

using namespace std;

const regex ID_REGEX("[A-Za-z_]\\w*");
const regex DEC_INT_REGEX("[1-9]\\d*");
const regex OCT_INT_REGEX("0[0-7]*");
const regex HEX_INT_REGEX("0[Xx][\\dA-Fa-f]+");
const regex FLOAT_REGEX(
    "((\\d+\\.(\\d+)?|\\.\\d+)([Ee][+-]?\\d+)?|\\d+([Ee][+-]?\\d+))|0[Xx](["
    "\\dA-Fa-f]+\\.?([\\dA-Fa-f]+)?|\\.[\\dA-Fa-f]+)[Pp][+-]?\\d+");

char buf[BUFSIZ];

unordered_map<string, Token::Type> KEYWORDS = {
    {"break", Token::BREAK},       {"const", Token::CONST},
    {"continue", Token::CONTINUE}, {"else", Token::ELSE},
    {"float", Token::FLOAT},       {"if", Token::IF},
    {"int", Token::INT},           {"return", Token::RETURN},
    {"void", Token::VOID},         {"while", Token::WHILE}};

LexicalParser::LexicalParser() {
  this->head = 0;
  this->lineno = 1;
}

void LexicalParser::readFile() {
  FILE *file = fopen(fileName.data(), "r");
  unsigned n;
  while ((n = fread(buf, 1, BUFSIZ, file)) != 0)
    content.append(buf, n);
  fclose(file);
}

void LexicalParser::setInput(const string &fileName) {
  this->fileName = fileName;
}

void LexicalParser::parse() {
  Token *token;
  readFile();
  while ((token = nextToken()) != nullptr)
    tokens.push_back(token);
}

Token *LexicalParser::nextToken() {
  Token *token;
  while (head < content.length() && isspace(content[head])) {
    if (content[head] == '\n')
      lineno++;
    head++;
  }
  if (head == content.length())
    return nullptr;
  switch (content[head]) {
  case '!':
    head++;
    if (content[head] != '=') {
      token = new Token(Token::L_NOT);
      return token;
    }
    token = new Token(Token::NE);
    head++;
    return token;
  case '%':
    token = new Token(Token::MOD);
    head++;
    return token;
  case '&':
    token = new Token(Token::L_AND);
    head += 2;
    return token;
  case '(':
    token = new Token(Token::LP);
    head++;
    return token;
  case ')':
    token = new Token(Token::RP);
    head++;
    return token;
  case '*':
    token = new Token(Token::MUL);
    head++;
    return token;
  case '+':
    token = new Token(Token::PLUS);
    head++;
    return token;
  case ',':
    token = new Token(Token::COMMA);
    head++;
    return token;
  case '-':
    token = new Token(Token::MINUS);
    head++;
    return token;
  case '/':
    switch (content[head + 1]) {
    case '*':
      head = content.find("*/", head + 2) + 2;
      return nextToken();
    case '/': {
      head = content.find('\n', head + 2);
      lineno++;
      if (head == string::npos) {
        head = content.length();
        return nextToken();
      }
      head++;
      while (content[head - 2] == '\\') {
        head = content.find('\n', head);
        lineno++;
        if (head == string::npos) {
          head = content.length();
          return nextToken();
        }
        head++;
      }
      return nextToken();
    }
    default:
      token = new Token(Token::DIV);
      head++;
      return token;
    }
  case ';':
    token = new Token(Token::SEMICOLON);
    head++;
    return token;
  case '<':
    head++;
    if (content[head] != '=') {
      token = new Token(Token::LT);
      return token;
    }
    token = new Token(Token::LE);
    head++;
    return token;
  case '=':
    head++;
    if (content[head] != '=') {
      token = new Token(Token::ASSIGN);
      return token;
    }
    token = new Token(Token::EQ);
    head++;
    return token;
  case '>':
    head++;
    if (content[head] != '=') {
      token = new Token(Token::GT);
      return token;
    }
    token = new Token(Token::GE);
    head++;
    return token;
  case '[':
    token = new Token(Token::LB);
    head++;
    return token;
  case ']':
    token = new Token(Token::RB);
    head++;
    return token;
  case '{':
    token = new Token(Token::LC);
    head++;
    return token;
  case '|':
    token = new Token(Token::L_OR);
    head += 2;
    return token;
  case '}':
    token = new Token(Token::RC);
    head++;
    return token;
  default: {
    unsigned tail = head;
    while (tail < content.length() &&
           (isalnum(content[tail]) || content[tail] == '_'))
      tail++;
    string tokenStr = content.substr(head, tail - head);
    if (regex_match(tokenStr, ID_REGEX)) {
      head = tail;
      if (KEYWORDS.find(tokenStr) != KEYWORDS.end())
        return new Token(KEYWORDS[tokenStr]);
      if (!tokenStr.compare("starttime"))
        startLineno = lineno;
      if (!tokenStr.compare("stoptime"))
        stopLineno = lineno;
      return new Token(tokenStr);
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
      head = tail;
      return new Token(stof(tokenStr));
    }
    tail = head;
    while (tail < content.length() &&
           (isxdigit(content[tail]) || content[tail] == 'X' ||
            content[tail] == 'x'))
      tail++;
    tokenStr = content.substr(head, tail - head);
    if (regex_match(tokenStr, HEX_INT_REGEX)) {
      head = tail;
      return new Token((int)stol(tokenStr, 0, 16));
    }
    tail = head;
    while (isdigit(content[tail]))
      tail++;
    tokenStr = content.substr(head, tail - head);
    if (regex_match(tokenStr, DEC_INT_REGEX)) {
      head = tail;
      return new Token((int)stol(tokenStr));
    }
    tail = head;
    while (content[tail] >= '0' && content[tail] <= '7')
      tail++;
    tokenStr = content.substr(head, tail - head);
    if (regex_match(tokenStr, OCT_INT_REGEX)) {
      head = tail;
      return new Token((int)stol(tokenStr, 0, 8));
    }
    break;
  }
  }
  return nullptr;
}
