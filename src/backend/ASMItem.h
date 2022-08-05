#ifndef __ASM_ITEM_H__
#define __ASM_ITEM_H__

#include <string>

#include "Reg.h"

class ASMItem {
public:
  enum ASMItemType {
    ASR,
    COND,
    FLOAT,
    INT,
    LABEL,
    LSL,
    LSR,
    OP,
    POSNEG,
    REG,
    TAG,
    WB
  };
  enum OpType { MINUS, PLUS };

  ASMItemType type;
  Reg::Type reg;
  OpType op;
  bool bVal;
  union {
    float fVal;
    int iVal;
  };
  std::string sVal;

  ASMItem(ASMItemType, int);
  ASMItem(Reg::Type);
  ASMItem(OpType);
  ASMItem(float);
  ASMItem(int);
  ASMItem(unsigned);
  ASMItem(const std::string &);
  ~ASMItem();
};

#endif