#ifndef __AST_H__
#define __AST_H__

#include <string>
#include <unordered_map>
#include <vector>

#include "Symbol.h"

using namespace std;

class Symbol;

class AST {
public:
  enum ASTType {
    ASSIGN_STMT,
    BINARY_EXP,
    BLANK_STMT,
    BLOCK,
    BREAK_STMT,
    CONST_DEF,
    CONST_INIT_VAL,
    CONTINUE_STMT,
    EXP_STMT,
    FLOAT_LITERAL,
    FUNC_CALL,
    FUNC_DEF,
    FUNC_PARAM,
    GLOBAL_VAR_DEF,
    IF_STMT,
    INIT_VAL,
    INT_LITERAL,
    LOCAL_VAR_DEF,
    L_VAL,
    MEMSET_ZERO,
    RETURN_STMT,
    ROOT,
    UNARY_EXP,
    WHILE_STMT
  };

  enum OPType {
    ADD,
    DIV,
    EQ,
    GE,
    GT,
    LE,
    LT,
    L_AND,
    L_NOT,
    L_OR,
    MOD,
    MUL,
    NE,
    NEG,
    POS,
    SUB
  };

  ASTType astType;
  OPType opType;
  Symbol *symbol;
  union {
    int iVal;
    float fVal;
  };
  vector<AST *> nodes;

  AST(ASTType);
  AST(ASTType, const vector<AST *> &);
  AST(ASTType, OPType);
  AST(ASTType, OPType, const vector<AST *> &);
  AST(ASTType, Symbol *);
  AST(ASTType, Symbol *, const vector<AST *> &);
  AST(float);
  AST(int);
  ~AST();
};

#endif