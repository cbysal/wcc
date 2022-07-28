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
    GE,
    GOTO,
    GT,
    I2F,
    L_NOT,
    LABEL,
    LABEL_WHILE_BEGIN,
    LABEL_WHILE_END,
    LE,
    LT,
    MEMSET_ZERO,
    MOD,
    MOV,
    MUL,
    NE,
    NEG,
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