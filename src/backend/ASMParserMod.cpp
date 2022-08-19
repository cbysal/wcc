#include "ASMParser.h"
#include "HardCoding.h"

using namespace std;

void ASMParser::parseMod(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP)
    parseModItempItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::ITEMP &&
           ir->items[2]->type == IRItem::INT)
    parseModItempInt(asms, ir);
  else if (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::ITEMP)
    parseModIntItemp(asms, ir);
}

void ASMParser::parseModIntItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  loadImmToReg(asms, Reg::A2, (unsigned)ir->items[1]->iVal);
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::DIV,
              {new ASMItem(Reg::A1), new ASMItem(Reg::A2),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  asms.push_back(new ASM(ASM::MUL, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                                    new ASMItem(Reg::A3)}));
  asms.push_back(
      new ASM(ASM::SUB,
              {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(Reg::A2),
               new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseModItempInt(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (ir->items[2]->iVal == INT32_MIN) {
    asms.push_back(new ASM(
        ASM::MOV,
        {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal])}));
    asms.push_back(new ASM(
        ASM::CMP, {new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(INT32_MIN)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ,
                {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                 new ASMItem(0)}));
  } else if (abs(ir->items[2]->iVal) == 1)
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(0)}));
  else {
    unsigned div = abs(ir->items[2]->iVal);
    unsigned shift = 0;
    while (1ull << (shift + 32) <= (0x7fffffff - 0x80000000 % div) *
                                       (div - (1ull << (shift + 32)) % div))
      shift++;
    unsigned magic = (1ull << (shift + 32)) / div + 1;
    loadImmToReg(asms, Reg::A1, magic);
    if (magic <= 0x7fffffff)
      asms.push_back(
          new ASM(ASM::SMMUL,
                  {new ASMItem(Reg::A1),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A1)}));
    else
      asms.push_back(new ASM(
          ASM::SMMLA,
          {new ASMItem(Reg::A1),
           new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
           new ASMItem(Reg::A1),
           new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal])}));
    if (shift)
      asms.push_back(
          new ASM(ASM::ASR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                             new ASMItem(shift)}));
    if (magic <= 0x7fffffff)
      asms.push_back(
          new ASM(ASM::ADD,
                  {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ASMItem::LSR, 31)}));
    else
      asms.push_back(
          new ASM(ASM::ADD,
                  {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ASMItem::LSR, 31)}));
    if (num2power2Map.find(ir->items[2]->iVal) != num2power2Map.end() &&
        (ir->items[2]->iVal & 0x1) == 0x1)
      asms.push_back(new ASM(
          ASM::ADD,
          {new ASMItem(Reg::A1), new ASMItem(Reg::A1), new ASMItem(Reg::A1),
           new ASMItem(ASMItem::LSL,
                       num2power2Map[ir->items[2]->iVal].second)}));
    else if (num2power2Map.find(ir->items[2]->iVal) != num2power2Map.end()) {
      asms.push_back(new ASM(
          ASM::ADD,
          {new ASMItem(Reg::A1), new ASMItem(Reg::A1), new ASMItem(Reg::A1),
           new ASMItem(ASMItem::LSL,
                       num2power2Map[ir->items[2]->iVal].second -
                           num2power2Map[ir->items[2]->iVal].first)}));
      asms.push_back(new ASM(
          ASM::LSL, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                     new ASMItem(num2power2Map[ir->items[2]->iVal].first)}));
    } else if (num2lineMap.find(ir->items[2]->iVal) != num2lineMap.end()) {
      asms.push_back(new ASM(
          ASM::RSB,
          {new ASMItem(Reg::A1), new ASMItem(Reg::A1), new ASMItem(Reg::A1),
           new ASMItem(ASMItem::LSL, num2lineMap[ir->items[2]->iVal].second -
                                         num2lineMap[ir->items[2]->iVal].first +
                                         1)}));
      asms.push_back(new ASM(
          ASM::LSL, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                     new ASMItem(num2lineMap[ir->items[2]->iVal].first)}));
    } else {
      loadImmToReg(asms, Reg::A3, (unsigned)ir->items[2]->iVal);
      asms.push_back(
          new ASM(ASM::MUL, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                             new ASMItem(Reg::A3)}));
    }
    asms.push_back(new ASM(
        ASM::SUB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A1)}));
  }
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseModItempItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::DIV,
              {new ASMItem(Reg::A1),
               new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  asms.push_back(
      new ASM(ASM::MUL,
              {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  asms.push_back(new ASM(
      ASM::SUB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                 new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                 new ASMItem(Reg::A1)}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}
