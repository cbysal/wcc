#ifndef __ASM_H__
#define __ASM_H__

#include <string>
#include <vector>

#include "ASMItem.h"

class ASM {
public:
  enum ASMOpType {
    ADD,
    AND,
    ASR,
    B,
    BL,
    CMN,
    CMP,
    DIV,
    DW,
    EOR,
    LABEL,
    LDR,
    LSL,
    MLA,
    MOV,
    MOVT,
    MOVW,
    MUL,
    MVN,
    NOP,
    POP,
    PUSH,
    ROR,
    RSB,
    SMMLA,
    SMMUL,
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
  std::vector<ASMItem *> items;

  std::string toString();

  ASM(ASMOpType, const std::vector<ASMItem *> &);
  ASM(ASMOpType, CondType, const std::vector<ASMItem *> &);
  ~ASM();
};

#endif