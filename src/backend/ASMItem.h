#ifndef __ASM_ITEM_H__
#define __ASM_ITEM_H__

#include <unordered_map>

using namespace std;

class ASMItem {
public:
  enum ASMItemType { COND, IMM, LABEL, OP, POSNEG, REG, TAG, WB };
  enum OpType { MINUS, PLUS };
  enum RegType {
    R0,
    A1 = R0,
    R1,
    A2 = R1,
    R2,
    A3 = R2,
    R3,
    A4 = R3,
    R4,
    V1 = R4,
    R5,
    V2 = R5,
    R6,
    V3 = R6,
    R7,
    V4 = R7,
    R8,
    V5 = R8,
    R9,
    V6 = R9,
    R10,
    V7 = R10,
    R11,
    V8 = R11,
    R12,
    IP = R12,
    R13,
    SP = R13,
    R14,
    LR = R14,
    R15,
    PC = R15,
    S0,
    S1,
    S2,
    S3,
    S4,
    S5,
    S6,
    S7,
    S8,
    S9,
    S10,
    S11,
    S12,
    S13,
    S14,
    S15,
    S16,
    S17,
    S18,
    S19,
    S20,
    S21,
    S22,
    S23,
    S24,
    S25,
    S26,
    S27,
    S28,
    S29,
    S30,
    S31
  };

  ASMItemType type;
  RegType reg;
  OpType op;
  bool bVal;
  union {
    float fVal;
    int iVal;
  };
  string sVal;

  ASMItem(ASMItemType, int);
  ASMItem(RegType);
  ASMItem(OpType);
  ASMItem(int);
  ASMItem(const string &);
  ~ASMItem();
};

extern unordered_map<ASMItem::RegType, string> regTypeStr;

#endif