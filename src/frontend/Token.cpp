#include "Token.h"

Token::Token(int line, int column, Type type) {
  this->line = line;
  this->column = column;
  this->type = type;
}

Token::Token(int line, int column, Type type, int value) {
  this->line = line;
  this->column = column;
  this->type = type;
  this->intValue = value;
}

Token::Token(int line, int column, Type type, float value) {
  this->line = line;
  this->column = column;
  this->type = type;
  this->floatValue = value;
}

Token::Token(int line, int column, Type type, string value) {
  this->line = line;
  this->column = column;
  this->type = type;
  this->stringValue = value;
}

Token::~Token() {}

const string Token::toString() {
  string s;
  s += '(';
  s += to_string(line);
  s += ", ";
  s += to_string(column);
  s += ", ";
  s += TYPE_STR.at(type);
  switch (type) {
  case FLOAT_LITERAL:
    s += ", ";
    s += to_string(floatValue);
    break;
  case ID:
    s += ", ";
    s += stringValue;
    break;
  case INT_LITERAL:
    s += ", ";
    s += to_string(intValue);
    break;
  default:
    break;
  }
  s += ')';
  return s;
}