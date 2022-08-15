#include "ASMParser.h"

using namespace std;

void ASMParser::parseB(vector<ASM *> &asms, IR *ir) {
  parseLCmp(asms, ir);
  switch (ir->type) {
  case IR::BEQ:
    asms.push_back(
        new ASM(ASM::B, ASM::EQ,
                {new ASMItem(ASMItem::LABEL, irLabels[ir->items[0]->ir])}));
    break;
  case IR::BGE:
    asms.push_back(
        new ASM(ASM::B, ASM::GE,
                {new ASMItem(ASMItem::LABEL, irLabels[ir->items[0]->ir])}));
    break;
  case IR::BGT:
    asms.push_back(
        new ASM(ASM::B, ASM::GT,
                {new ASMItem(ASMItem::LABEL, irLabels[ir->items[0]->ir])}));
    break;
  case IR::BLE:
    asms.push_back(
        new ASM(ASM::B, ASM::LE,
                {new ASMItem(ASMItem::LABEL, irLabels[ir->items[0]->ir])}));
    break;
  case IR::BLT:
    asms.push_back(
        new ASM(ASM::B, ASM::LT,
                {new ASMItem(ASMItem::LABEL, irLabels[ir->items[0]->ir])}));
    break;
  case IR::BNE:
    asms.push_back(
        new ASM(ASM::B, ASM::NE,
                {new ASMItem(ASMItem::LABEL, irLabels[ir->items[0]->ir])}));
    break;
  default:
    break;
  }
}

void ASMParser::parseCmp(vector<ASM *> &asms, IR *ir) {
  parseLCmp(asms, ir);
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end();
  Reg::Type targetReg = flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal];
  switch (ir->type) {
  case IR::EQ:
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::NE, {new ASMItem(targetReg), new ASMItem(0)}));
    break;
  case IR::GE:
    asms.push_back(
        new ASM(ASM::MOV, ASM::GE, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::LT, {new ASMItem(targetReg), new ASMItem(0)}));
    break;
  case IR::GT:
    asms.push_back(
        new ASM(ASM::MOV, ASM::GT, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::LE, {new ASMItem(targetReg), new ASMItem(0)}));
    break;
  case IR::LE:
    asms.push_back(
        new ASM(ASM::MOV, ASM::LE, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::GT, {new ASMItem(targetReg), new ASMItem(0)}));
    break;
  case IR::LT:
    asms.push_back(
        new ASM(ASM::MOV, ASM::LT, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::GE, {new ASMItem(targetReg), new ASMItem(0)}));
    break;
  case IR::NE:
    asms.push_back(
        new ASM(ASM::MOV, ASM::NE, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ, {new ASMItem(targetReg), new ASMItem(0)}));
    break;
  default:
    break;
  }
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseLCmp(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP)
    parseLCmpItempItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::ITEMP &&
           ir->items[2]->type == IRItem::INT)
    parseLCmpItempInt(asms, ir);
  else if (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::ITEMP) {
    switchLCmpLogic(ir);
    parseLCmpItempInt(asms, ir);
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FTEMP)
    parseLCmpFtempFtemp(asms, ir);
  else if (ir->items[1]->type == IRItem::FTEMP &&
           ir->items[2]->type == IRItem::FLOAT)
    parseLCmpFtempFloat(asms, ir);
  else if (ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FTEMP) {
    switchLCmpLogic(ir);
    parseAddFtempFloat(asms, ir);
  }
}

void ASMParser::parseLCmpFtempFloat(vector<ASM *> &asms, IR *ir) {
  bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S0, spillOffsets[ir->items[1]->iVal]);
  if (ir->items[2]->fVal) {
    loadImmToReg(asms, Reg::S1, ir->items[2]->fVal);
    asms.push_back(
        new ASM(ASM::VCMP,
                {new ASMItem(flag2 ? Reg::S0 : ftemp2Reg[ir->items[1]->iVal]),
                 new ASMItem(Reg::S1)}));
  } else
    asms.push_back(
        new ASM(ASM::VCMP,
                {new ASMItem(flag2 ? Reg::S0 : ftemp2Reg[ir->items[1]->iVal]),
                 new ASMItem(0.0f)}));
  asms.push_back(new ASM(ASM::VMRS, {}));
}

void ASMParser::parseLCmpFtempFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S0, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VCMP,
              {new ASMItem(flag2 ? Reg::S0 : ftemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::S1 : ftemp2Reg[ir->items[2]->iVal])}));
  asms.push_back(new ASM(ASM::VMRS, {}));
}

void ASMParser::parseLCmpItempInt(vector<ASM *> &asms, IR *ir) {
  bool flag = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag)
    loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
  if (isByteShiftImm(ir->items[2]->iVal))
    asms.push_back(new ASM(
        ASM::CMP, {new ASMItem(flag ? Reg::A1 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ir->items[2]->iVal)}));
  else if (isByteShiftImm(-ir->items[2]->iVal))
    asms.push_back(new ASM(
        ASM::CMN, {new ASMItem(flag ? Reg::A1 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(-ir->items[2]->iVal)}));
  else if (canBeLoadInSingleInstruction(ir->items[2]->iVal) ||
           canBeLoadInSingleInstruction(-ir->items[2]->iVal)) {
    loadImmToReg(asms, Reg::A2, (unsigned)ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::CMP, {new ASMItem(flag ? Reg::A1 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A2)}));
  } else {
    loadImmToReg(asms, Reg::A2, (unsigned)(-ir->items[2]->iVal));
    asms.push_back(new ASM(
        ASM::CMN, {new ASMItem(flag ? Reg::A1 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A2)}));
  }
}

void ASMParser::parseLCmpItempItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  if (flag1)
    loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::CMP,
              {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[2]->iVal])}));
}

void ASMParser::switchLCmpLogic(IR *ir) {
  switch (ir->type) {
  case IR::BEQ:
    ir->type = IR::BEQ;
    break;
  case IR::BGE:
    ir->type = IR::BLE;
    break;
  case IR::BGT:
    ir->type = IR::BLT;
    break;
  case IR::BLE:
    ir->type = IR::BGE;
    break;
  case IR::BLT:
    ir->type = IR::BGT;
    break;
  case IR::BNE:
    ir->type = IR::BNE;
    break;
  case IR::EQ:
    ir->type = IR::EQ;
    break;
  case IR::GE:
    ir->type = IR::LE;
    break;
  case IR::GT:
    ir->type = IR::LT;
    break;
  case IR::LE:
    ir->type = IR::GE;
    break;
  case IR::LT:
    ir->type = IR::GT;
    break;
  case IR::NE:
    ir->type = IR::NE;
    break;
  default:
    break;
  }
  swap(ir->items[1], ir->items[2]);
}
