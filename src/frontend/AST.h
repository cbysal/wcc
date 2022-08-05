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
    ERR_TYPE,
    ADD_EXP,
    ASSIGN_STMT,
    BLANK_STMT,
    BLOCK,
    BREAK_STMT,
    CONST_DEF,
    CONST_INIT_VAL,
    CONTINUE_STMT,
    DIV_EXP,
    EQ_EXP,
    EXP_STMT,
    F2I_EXP,
    FLOAT,
    FUNC_CALL,
    FUNC_DEF,
    FUNC_PARAM,
    GE_EXP,
    GT_EXP,
    GLOBAL_VAR_DEF,
    I2F_EXP,
    IF_STMT,
    INIT_VAL,
    INT,
    LE_EXP,
    LOCAL_VAR_DEF,
    LT_EXP,
    L_AND_EXP,
    L_NOT_EXP,
    L_OR_EXP,
    L_VAL,
    MEMSET_ZERO,
    MOD_EXP,
    MUL_EXP,
    NE_EXP,
    NEG_EXP,
    POS_EXP,
    RETURN_STMT,
    ROOT,
    SUB_EXP,
    WHILE_STMT
  };

  ASTType type;
  bool isFloat;
  Symbol *symbol;
  union {
    int iVal;
    float fVal;
  };
  vector<AST *> nodes;

  AST(ASTType, bool, Symbol *, const vector<AST *> &);
  AST(ASTType, bool, const vector<AST *> &);
  AST(float);
  AST(int);
  ~AST();

  AST *transIF();
};

#endif