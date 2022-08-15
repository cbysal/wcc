#include "ASMParser.h"
#include "HardCoding.h"

using namespace std;

void ASMParser::parseMul(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP)
    parseMulItempItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::ITEMP &&
           ir->items[2]->type == IRItem::INT)
    parseMulItempInt(asms, ir);
  else if (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::ITEMP) {
    swap(ir->items[1], ir->items[2]);
    parseMulItempInt(asms, ir);
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FTEMP)
    parseMulFtempFtemp(asms, ir);
  else if (ir->items[1]->type == IRItem::FTEMP &&
           ir->items[2]->type == IRItem::FLOAT)
    parseMulFtempFloat(asms, ir);
  else if (ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FTEMP)
    parseMulFloatFtemp(asms, ir);
}

void ASMParser::parseMulFloatFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  loadImmToReg(asms, Reg::S1, ir->items[1]->fVal);
  if (flag3)
    loadFromSP(asms, Reg::S2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VMUL,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(Reg::S1),
               new ASMItem(flag3 ? Reg::S2 : ftemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseMulFtempFloat(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  loadImmToReg(asms, Reg::S2, ir->items[2]->fVal);
  asms.push_back(new ASM(
      ASM::VMUL, {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
                  new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
                  new ASMItem(Reg::S2)}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseMulFtempFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::S2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VMUL,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::S2 : ftemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseMulItempInt(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (!ir->items[2]->iVal)
    loadImmToReg(asms, flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal], 0u);
  else if (ir->items[2]->iVal == 1)
    asms.push_back(new ASM(
        ASM::MOV,
        {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal])}));
  else if (ir->items[2]->iVal == -1)
    asms.push_back(new ASM(
        ASM::RSB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(0)}));
  else if (num2powerMap.find(ir->items[2]->iVal) != num2powerMap.end())
    asms.push_back(new ASM(
        ASM::LSL, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(num2powerMap[ir->items[2]->iVal])}));
  else if (num2power2Map.find(ir->items[2]->iVal) != num2power2Map.end() &&
           (ir->items[2]->iVal & 0x1) == 0x1)
    asms.push_back(new ASM(
        ASM::ADD,
        {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::LSL, num2power2Map[ir->items[2]->iVal].second)}));
  else if (num2lineMap.find(ir->items[2]->iVal) != num2lineMap.end() &&
           !num2lineMap[ir->items[2]->iVal].first)
    asms.push_back(new ASM(
        ASM::RSB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ASMItem::LSL,
                               num2lineMap[ir->items[2]->iVal].second + 1)}));
  else if (num2power2Map.find(ir->items[2]->iVal) != num2power2Map.end()) {
    asms.push_back(new ASM(
        ASM::ADD, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ASMItem::LSL,
                               num2power2Map[ir->items[2]->iVal].second -
                                   num2power2Map[ir->items[2]->iVal].first)}));
    asms.push_back(new ASM(
        ASM::LSL, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(num2power2Map[ir->items[2]->iVal].first)}));
  } else if (num2lineMap.find(ir->items[2]->iVal) != num2lineMap.end()) {
    asms.push_back(
        new ASM(ASM::RSB,
                {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                 new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                 new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                 new ASMItem(ASMItem::LSL,
                             num2lineMap[ir->items[2]->iVal].second -
                                 num2lineMap[ir->items[2]->iVal].first + 1)}));
    asms.push_back(new ASM(
        ASM::LSL, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(num2lineMap[ir->items[2]->iVal].first)}));
  } else {
    loadImmToReg(asms, Reg::A3, (unsigned)ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::MUL, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A3)}));
  }
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseMulItempItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::MUL,
              {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}
