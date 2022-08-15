#include "ASMParser.h"

using namespace std;

void ASMParser::parseSub(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP)
    parseSubItempItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::ITEMP &&
           ir->items[2]->type == IRItem::INT)
    parseSubItempInt(asms, ir);
  else if (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::ITEMP)
    parseSubIntItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::FTEMP &&
           ir->items[2]->type == IRItem::FTEMP)
    parseSubFtempFtemp(asms, ir);
  else if (ir->items[1]->type == IRItem::FTEMP &&
           ir->items[2]->type == IRItem::FLOAT)
    parseSubFtempFloat(asms, ir);
  else if (ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FTEMP)
    parseSubFloatFtemp(asms, ir);
}

void ASMParser::parseSubFloatFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  loadImmToReg(asms, Reg::S1, ir->items[1]->fVal);
  if (flag3)
    loadFromSP(asms, Reg::S2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VSUB,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(Reg::S1),
               new ASMItem(flag3 ? Reg::S2 : ftemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseSubFtempFloat(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  loadImmToReg(asms, Reg::S2, ir->items[2]->fVal);
  asms.push_back(new ASM(
      ASM::VSUB, {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
                  new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
                  new ASMItem(Reg::S2)}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseSubFtempFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::S2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VSUB,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::S2 : ftemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseSubIntItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  if (isByteShiftImm(ir->items[1]->iVal)) {
    asms.push_back(new ASM(
        ASM::RSB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal]),
                   new ASMItem(ir->items[1]->iVal)}));
  } else if (canBeLoadInSingleInstruction(ir->items[1]->iVal)) {
    loadImmToReg(asms, Reg::A2, (unsigned)ir->items[1]->iVal);
    asms.push_back(new ASM(
        ASM::SUB,
        {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(Reg::A2),
         new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  } else if (canBeLoadInSingleInstruction(-ir->items[1]->iVal)) {
    loadImmToReg(asms, Reg::A2, (unsigned)(-ir->items[1]->iVal));
    asms.push_back(new ASM(
        ASM::ADD,
        {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(Reg::A2),
         new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  } else {
    loadImmToReg(asms, Reg::A3, (unsigned)ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::SUB,
        {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(Reg::A2),
         new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  }
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseSubItempInt(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (isByteShiftImm(ir->items[2]->iVal)) {
    asms.push_back(new ASM(
        ASM::SUB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ir->items[2]->iVal)}));
  } else if (isByteShiftImm(-ir->items[2]->iVal)) {
    asms.push_back(new ASM(
        ASM::ADD, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(-ir->items[2]->iVal)}));
  } else if (canBeLoadInSingleInstruction(ir->items[2]->iVal)) {
    loadImmToReg(asms, Reg::A3, (unsigned)ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::SUB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A3)}));
  } else if (canBeLoadInSingleInstruction(-ir->items[2]->iVal)) {
    loadImmToReg(asms, Reg::A3, (unsigned)(-ir->items[2]->iVal));
    asms.push_back(new ASM(
        ASM::ADD, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A3)}));
  } else {
    loadImmToReg(asms, Reg::A3, (unsigned)ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::SUB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A3)}));
  }
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseSubItempItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::SUB,
              {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}
