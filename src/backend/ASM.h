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
    DIV,
    DW,
    EOR,
    LABEL,
    LDR,
    MOV,
    MOVT,
    MOVW,
    MUL,
    POP,
    PUSH,
    RSB,
    STR,
    SUB,
    TAG,
    VADD,
    VCMP,
    VCVTFS,
    VCVTSF,
    VDIV,
    VLDR,
    VMOV,
    VMRS,
    VMUL,
    VNEG,
    VPOP,
    VPUSH,
    VSTR,
    VSUB
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