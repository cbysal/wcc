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
    FUNC_END,
    GOTO,
    LABEL_WHILE_BEGIN,
    LABEL_WHILE_END,
    LOAD,
    MEMSET_ZERO,
    MOD,
    MOV,
    MUL,
    NEG,
    PLT,
    POP,
    POS,
    PUSH,
    RETURN,
    STORE,
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