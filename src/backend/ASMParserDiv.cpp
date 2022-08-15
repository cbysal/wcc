#include "ASMParser.h"

using namespace std;

void ASMParser::parseDiv(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP)
    parseDivItempItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::ITEMP &&
           ir->items[2]->type == IRItem::INT)
    parseDivItempInt(asms, ir);
  else if (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::ITEMP)
    parseDivIntItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::FTEMP &&
           ir->items[2]->type == IRItem::FTEMP)
    parseDivFtempFtemp(asms, ir);
  else if (ir->items[1]->type == IRItem::FTEMP &&
           ir->items[2]->type == IRItem::FLOAT)
    parseDivFtempFloat(asms, ir);
  else if (ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FTEMP)
    parseDivFloatFtemp(asms, ir);
}

void ASMParser::parseDivFloatFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  loadImmToReg(asms, Reg::S1, ir->items[1]->fVal);
  if (flag3)
    loadFromSP(asms, Reg::S2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VDIV,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(Reg::S1),
               new ASMItem(flag3 ? Reg::S2 : ftemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseDivFtempFloat(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (ir->items[2]->fVal == 1.0f) {
    if (flag1 && flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !flag2)
      storeFromSP(asms, ftemp2Reg[ir->items[1]->iVal],
                  spillOffsets[ir->items[0]->iVal]);
    else if (!flag1 && flag2)
      loadFromSP(asms, ftemp2Reg[ir->items[0]->iVal],
                 spillOffsets[ir->items[1]->iVal]);
    else if (!flag1 && !flag2)
      asms.push_back(
          new ASM(ASM::VMOV, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                              new ASMItem(ftemp2Reg[ir->items[0]->iVal])}));
    return;
  }
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  if (ir->items[2]->fVal == -1.0f) {
    if (flag1)
      storeFromSP(asms, flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal],
                  spillOffsets[ir->items[0]->iVal]);
    else
      asms.push_back(new ASM(
          ASM::VNEG,
          {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
           new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal])}));
    return;
  }
  loadImmToReg(asms, Reg::S2, ir->items[2]->fVal);
  asms.push_back(new ASM(
      ASM::VDIV, {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
                  new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
                  new ASMItem(Reg::S2)}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseDivFtempFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::S2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VDIV,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::S2 : ftemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseDivIntItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  loadImmToReg(asms, Reg::A2, (unsigned)ir->items[1]->iVal);
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::DIV,
              {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(Reg::A2),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseDivItempInt(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (ir->items[2]->iVal == 1)
    asms.push_back(new ASM(
        ASM::MOV,
        {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal])}));
  else if (ir->items[2]->iVal == -1)
    asms.push_back(new ASM(
        ASM::RSB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(0)}));
  else if (ir->items[2]->iVal == INT32_MIN)
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(0)}));
  else {
    unsigned div = ir->items[2]->iVal;
    bool flag = true;
    if (((int)div) < 0) {
      flag = false;
      div = abs(((int)div));
    }
    unsigned shift = 0;
    while (1ull << (shift + 32) <= (0x7fffffff - 0x80000000 % div) *
                                       (div - (1ull << (shift + 32)) % div))
      shift++;
    unsigned magic = (1ull << (shift + 32)) / div + 1;
    loadImmToReg(asms, flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal], magic);
    if (magic <= 0x7fffffff)
      asms.push_back(new ASM(
          ASM::SMMUL,
          {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
           new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal])}));
    else
      asms.push_back(new ASM(
          ASM::SMMLA,
          {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
           new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal])}));
    if (shift)
      asms.push_back(
          new ASM(ASM::ASR,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(shift)}));
    if (flag && magic <= 0x7fffffff)
      asms.push_back(
          new ASM(ASM::ADD,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ASMItem::LSR, 31)}));
    else if (flag && magic > 0x7fffffff)
      asms.push_back(
          new ASM(ASM::ADD,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ASMItem::LSR, 31)}));
    else if (!flag && magic <= 0x7fffffff)
      asms.push_back(
          new ASM(ASM::RSB,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ASMItem::ASR, 31)}));
    else if (!flag && magic > 0x7fffffff)
      asms.push_back(
          new ASM(ASM::RSB,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(Reg::A2), new ASMItem(ASMItem::ASR, 31)}));
  }
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseDivItempItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::DIV,
              {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseF2I(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S0, spillOffsets[ir->items[1]->iVal]);
  asms.push_back(
      new ASM(ASM::VCVTFS,
              {new ASMItem(Reg::S0),
               new ASMItem(flag2 ? Reg::S0 : ftemp2Reg[ir->items[1]->iVal])}));
  asms.push_back(new ASM(
      ASM::VMOV, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                  new ASMItem(Reg::S0)}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}
