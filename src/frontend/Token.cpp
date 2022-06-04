#include "Token.h"

using namespace std;

unordered_map<Token::Type, string> TokenTypeStr = {
    {Token::ASSIGN, "ASSIGN"},
    {Token::BREAK, "BREAK"},
    {Token::COMMA, "COMMA"},
    {Token::CONST, "CONST"},
    {Token::CONTINUE, "CONTINUE"},
    {Token::DIV, "DIV"},
    {Token::ELSE, "ELSE"},
    {Token::EQ, "EQ"},
    {Token::FLOAT, "FLOAT"},
    {Token::FLOAT_LITERAL, "FLOAT_LITERAL"},
    {Token::GE, "GE"},
    {Token::GT, "GT"},
    {Token::ID, "ID"},
    {Token::IF, "IF"},
    {Token::INT, "INT"},
    {Token::INT_LITERAL, "INT_LITERAL"},
    {Token::LB, "LB"},
    {Token::LC, "LC"},
    {Token::LE, "LE"},
    {Token::LP, "LP"},
    {Token::LT, "LT"},
    {Token::L_AND, "L_AND"},
    {Token::L_NOT, "L_NOT"},
    {Token::L_OR, "L_OR"},
    {Token::MINUS, "MINUS"},
    {Token::MOD, "MOD"},
    {Token::MUL, "MUL"},
    {Token::NE, "NE"},
    {Token::PLUS, "PLUS"},
    {Token::RB, "RB"},
    {Token::RC, "RC"},
    {Token::RETURN, "RETURN"},
    {Token::RP, "RP"},
    {Token::SEMICOLON, "SEMICOLON"},
    {Token::VOID, "VOID"},
    {Token::WHILE, "WHILE"}};

Token::Token(Type type) { this->type = type; }

Token::Token(float fVal) {
  this->type = FLOAT_LITERAL;
  this->fVal = fVal;
}

Token::Token(int iVal) {
  this->type = INT_LITERAL;
  this->iVal = iVal;
}

Token::Token(string &sVal) {
  this->type = ID;
  this->sVal = sVal;
}

Token::~Token() {}

string Token::toString() {
  switch (type) {
  case FLOAT_LITERAL:
    return "(" + TokenTypeStr[type] + ", " + to_string(fVal) + ")";
  case ID:
    return "(" + TokenTypeStr[type] + ", " + sVal + ")";
  case INT_LITERAL:
    return "(" + TokenTypeStr[type] + ", " + to_string(iVal) + ")";
  default:
    return "(" + TokenTypeStr[type] + ")";
  }
}