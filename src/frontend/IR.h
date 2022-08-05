#ifndef __IR_H__
#define __IR_H__

#include <string>
#include <vector>

#include "IRItem.h"
#include "Symbol.h"

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
  std::vector<IRItem *> items;

  std::string toString();

  IR(IRType);
  IR(IRType, const std::vector<IRItem *> &);
  ~IR();
};

#endif