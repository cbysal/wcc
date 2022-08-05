#ifndef __TOKEN_H__
#define __TOKEN_H__

#include <string>

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
    FLOAT_LIT,
    GE,
    GT,
    ID,
    IF,
    INT,
    INT_LIT,
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
  union {
    int iVal;
    float fVal;
  };
  std::string sVal;

  Token(Type);
  Token(float);
  Token(int);
  Token(const std::string &);
  ~Token();
};

#endif