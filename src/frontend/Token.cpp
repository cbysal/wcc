#include "Token.h"

using namespace std;

Token::Token(Type type) { this->type = type; }

Token::Token(float fVal) {
  this->type = FLOAT_LITERAL;
  this->fVal = fVal;
}

Token::Token(int iVal) {
  this->type = INT_LITERAL;
  this->iVal = iVal;
}

Token::Token(const string &sVal) {
  this->type = ID;
  this->sVal = sVal;
}

Token::~Token() {}