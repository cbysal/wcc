#ifndef __TOKEN_H__
#define __TOKEN_H__

#include <string>
#include <unordered_map>

using namespace std;

class Token {
public:
  enum Type {
    ASSIGN,
    BREAK,
    COMMA,
    CONST,
    CONTINUE,
    DIV,
    ELSE,
    EQ,
    FLOAT,
    FLOAT_LITERAL,
    GE,
    GT,
    ID,
    IF,
    INT,
    INT_LITERAL,
    LB,
    LC,
    LE,
    LP,
    LT,
    L_AND,
    L_NOT,
    L_OR,
    MINUS,
    MOD,
    MUL,
    NE,
    PLUS,
    RB,
    RC,
    RETURN,
    RP,
    SEMICOLON,
    VOID,
    WHILE
  };

  Type type;
  int iVal;
  float fVal;
  string sVal;

  Token(Type);
  Token(float);
  Token(int);
  Token(string &);
  ~Token();

  string toString();
};

#endif