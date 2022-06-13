#ifndef __ASM_H__
#define __ASM_H__

#include <string>
#include <vector>

#include "ASMItem.h"

using namespace std;

class ASM {
public:
  enum ASMOpType {
    ADD,
    B,
    BL,
    CMP,
    DW,
    LABEL,
    LDR,
    MOV,
    MOVT,
    MUL,
    POP,
    PUSH,
    RSB,
    STR,
    SUB,
    TAG
  };
  enum CondType {
    AL,
    CC,
    LO = CC,
    CS,
    HS = CS,
    EQ,
    GE,
    GT,
    HI,
    LE,
    LS,
    LT,
    MI,
    NE,
    PL,
    VC,
    VS
  };

  ASMOpType type;
  CondType cond;
  vector<ASMItem *> items;

  string toString();

  ASM(ASMOpType, const vector<ASMItem *> &);
  ASM(ASMOpType, CondType, const vector<ASMItem *> &);
  ~ASM();
};

#endif