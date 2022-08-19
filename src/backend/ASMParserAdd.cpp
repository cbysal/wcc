#include "ASMParser.h"

using namespace std;

void ASMParser::parseAdd(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP)
    parseAddItempItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::ITEMP &&
           ir->items[2]->type == IRItem::INT)
    parseAddItempInt(asms, ir);
  else if (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::ITEMP) {
    swap(ir->items[1], ir->items[2]);
    parseAddItempInt(asms, ir);
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FTEMP)
    parseAddFtempFtemp(asms, ir);
  else if (ir->items[1]->type == IRItem::FTEMP &&
           ir->items[2]->type == IRItem::FLOAT)
    parseAddFtempFloat(asms, ir);
  else if (ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FTEMP) {
    swap(ir->items[1], ir->items[2]);
    parseAddFtempFloat(asms, ir);
  }
}

void ASMParser::parseAddFtempFloat(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  if (canBeLoadInSingleInstruction(float2Unsigned(ir->items[2]->fVal)) ||
      !canBeLoadInSingleInstruction(float2Unsigned(-ir->items[2]->fVal))) {
    loadImmToReg(asms, Reg::S2, ir->items[2]->fVal);
    asms.push_back(
        new ASM(ASM::VADD,
                {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
                 new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
                 new ASMItem(Reg::S2)}));
  } else {
    loadImmToReg(asms, Reg::S2, -ir->items[2]->fVal);
    asms.push_back(
        new ASM(ASM::VSUB,
                {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
                 new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
                 new ASMItem(Reg::S2)}));
  }
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseAddFtempFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::S2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VADD,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::S2 : ftemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseAddItempInt(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (isByteShiftImm(ir->items[2]->iVal)) {
    asms.push_back(new ASM(
        ASM::ADD, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ir->items[2]->iVal)}));
  } else if (isByteShiftImm(-ir->items[2]->iVal)) {
    asms.push_back(new ASM(
        ASM::SUB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(-ir->items[2]->iVal)}));
  } else if (canBeLoadInSingleInstruction(ir->items[2]->iVal)) {
    loadImmToReg(asms, Reg::A3, (unsigned)ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::ADD, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A3)}));
  } else if (canBeLoadInSingleInstruction(-ir->items[2]->iVal)) {
    loadImmToReg(asms, Reg::A3, (unsigned)(-ir->items[2]->iVal));
    asms.push_back(new ASM(
        ASM::SUB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A3)}));
  } else {
    loadImmToReg(asms, Reg::A3, (unsigned)ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::ADD, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A3)}));
  }
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseAddItempItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::ADD,
              {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}