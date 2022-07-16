#ifndef __IR_H__
#define __IR_H__

#include "IRItem.h"
#include "Symbol.h"

using namespace std;

class IRItem;

class IR {
public:
  enum IRType {
    ADD,
    BEQ,
    BGE,
    BGT,
    BLE,
    BLT,
    BNE,
    BREAK,
    CALL,
    CONTINUE,
    DIV,
    EQ,
    F2I,
    FUNC_END,
    GE,
    GOTO,
    GT,
    I2F,
    L_NOT,
    LABEL_WHILE_BEGIN,
    LABEL_WHILE_END,
    LE,
    LT,
    MEMSET_ZERO,
    MOV,
    MUL,
    NE,
    NEG,
    RETURN,
    SUB
  };

  int irId;
  IRType type;
  vector<IRItem *> items;

  string toString();

  IR(IRType);
  IR(IRType, const vector<IRItem *> &);
  ~IR();
};

#endif