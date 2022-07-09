#include <iostream>

#include "ASMParser.h"

using namespace std;

#define MAX_IMM8 0xff
#define MAX_IMM16 0xffff
#define ZERO16 0x10000

ASMParser::ASMParser(const string &asmFile, pair<unsigned, unsigned> lineno,
                     vector<pair<Symbol *, vector<IR *>>> &funcIRs,
                     vector<Symbol *> &consts, vector<Symbol *> &globalVars,
                     unordered_map<Symbol *, vector<Symbol *>> &localVars) {
  this->labelId = 0;
  this->asmFile = asmFile;
  this->funcIRs = funcIRs;
  this->consts = consts;
  this->globalVars = globalVars;
  this->localVars = localVars;
  this->startLineno = lineno.first;
  this->stopLineno = lineno.second;
}

ASMParser::~ASMParser() {
  for (const pair<Symbol *, vector<ASM *>> &funcASM : funcASMs)
    for (ASM *a : funcASM.second)
      delete a;
}

pair<int, int> ASMParser::allocReg(const vector<IR *> &irs) {
  itemp2Reg.clear();
  ftemp2Reg.clear();
  temp2SpillReg.clear();
  spillSize = 0;
  int iUsed = 0, fUsed = 0;
  vector<ASMItem::RegType> freeVRegs = {ASMItem::V8, ASMItem::V7, ASMItem::V6,
                                        ASMItem::V5, ASMItem::V4, ASMItem::V3,
                                        ASMItem::V2, ASMItem::V1};
  vector<ASMItem::RegType> freeSRegs = {
      ASMItem::S31, ASMItem::S30, ASMItem::S29, ASMItem::S28,
      ASMItem::S27, ASMItem::S26, ASMItem::S25, ASMItem::S24,
      ASMItem::S23, ASMItem::S22, ASMItem::S21, ASMItem::S20,
      ASMItem::S19, ASMItem::S18, ASMItem::S17, ASMItem::S16};
  unordered_map<int, unsigned> lifespan;
  unordered_map<unsigned, ASMItem::RegType> tempVReg, tempSReg;
  unordered_map<unsigned, int> tempSpillReg;
  for (unsigned i = 0; i < irs.size(); i++)
    for (IRItem *item : irs[i]->items)
      if (item->type == IRItem::FTEMP || item->type == IRItem::ITEMP)
        lifespan[item->iVal] = i;
  unordered_set<int> usedSpillRegs;
  for (unsigned i = 0; i < irs.size(); i++) {
    vector<unsigned> removeList;
    for (const pair<unsigned, ASMItem::RegType> reg : tempVReg)
      if (i > lifespan[reg.first]) {
        freeVRegs.push_back(tempVReg[reg.first]);
        removeList.push_back(reg.first);
      }
    for (unsigned id : removeList)
      tempVReg.erase(id);
    removeList.clear();
    for (const pair<unsigned, ASMItem::RegType> reg : tempSReg)
      if (i > lifespan[reg.first]) {
        freeSRegs.push_back(tempSReg[reg.first]);
        removeList.push_back(reg.first);
      }
    for (unsigned id : removeList)
      tempSReg.erase(id);
    removeList.clear();
    for (const pair<unsigned, int> reg : tempSpillReg)
      if (i > lifespan[reg.first]) {
        usedSpillRegs.erase(reg.second);
        removeList.push_back(reg.first);
      }
    for (unsigned id : removeList)
      tempSpillReg.erase(id);
    for (IRItem *item : irs[i]->items) {
      if (item->type == IRItem::ITEMP &&
          tempVReg.find(item->iVal) == tempVReg.end() &&
          tempSpillReg.find(item->iVal) == tempSpillReg.end()) {
        if (freeVRegs.empty()) {
          if (usedSpillRegs.size() == spillSize) {
            tempSpillReg[item->iVal] = spillSize;
            temp2SpillReg[item->iVal] = spillSize;
            usedSpillRegs.insert(spillSize);
            spillSize++;
          } else {
            for (unsigned j = 0; j < spillSize; j++)
              if (usedSpillRegs.find(j) == usedSpillRegs.end()) {
                tempSpillReg[item->iVal] = j;
                temp2SpillReg[item->iVal] = j;
                usedSpillRegs.insert(j);
                break;
              }
          }
        } else {
          tempVReg[item->iVal] = freeVRegs.back();
          itemp2Reg[item->iVal] = freeVRegs.back();
          freeVRegs.pop_back();
          iUsed = max<int>(iUsed, 8 - freeVRegs.size());
        }
      }
      if (item->type == IRItem::FTEMP &&
          tempSReg.find(item->iVal) == tempSReg.end() &&
          tempSpillReg.find(item->iVal) == tempSpillReg.end()) {
        if (freeSRegs.empty()) {
          if (usedSpillRegs.size() == spillSize) {
            tempSpillReg[item->iVal] = spillSize;
            temp2SpillReg[item->iVal] = spillSize;
            usedSpillRegs.insert(spillSize);
            spillSize++;
          } else {
            for (unsigned j = 0; j < spillSize; j++)
              if (usedSpillRegs.find(j) == usedSpillRegs.end()) {
                tempSpillReg[item->iVal] = j;
                temp2SpillReg[item->iVal] = j;
                usedSpillRegs.insert(j);
                break;
              }
          }
        } else {
          tempSReg[item->iVal] = freeSRegs.back();
          ftemp2Reg[item->iVal] = freeSRegs.back();
          freeSRegs.pop_back();
          fUsed = max<int>(fUsed, 16 - freeSRegs.size());
        }
      }
    }
  }
  spillSize *= 4;
  return {iUsed, fUsed};
}

int ASMParser::calcArgs(const vector<IR *> &irs) {
  int size = 0;
  for (IR *ir : irs) {
    if (ir->type == IR::CALL) {
      int iCnt = 0, fCnt = 0;
      for (Symbol *param : ir->items[0]->symbol->params) {
        if (!param->dimensions.empty() || param->dataType == Symbol::INT)
          iCnt++;
        else
          fCnt++;
      }
      size = max(size, max(iCnt - 4, 0) + max(fCnt - 16, 0));
    }
  }
  return size * 4;
}

void ASMParser::parse() {
  preProcess();
  for (const pair<Symbol *, vector<IR *>> &funcIR : funcIRs) {
    vector<ASM *> asms = parseFunc(funcIR.first, funcIR.second);
    removeUnusedLabels(asms);
    funcASMs.emplace_back(funcIR.first, asms);
  }
}

void ASMParser::parseAdd(vector<ASM *> &asms, IR *ir) {
  bool flag1 = false, flag2 = false, flag3 = false;
  if (ir->items[0]->type == IRItem::ITEMP) {
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
      flag1 = true;
    if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end()) {
      flag2 = true;
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    }
    if (itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end()) {
      flag3 = true;
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[2]->iVal])}));
    }
    asms.push_back(new ASM(
        ASM::ADD,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::A3 : itemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  } else {
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
      flag1 = true;
    if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end()) {
      flag2 = true;
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S1), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    }
    if (ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end()) {
      flag3 = true;
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S2), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[2]->iVal])}));
    }
    asms.push_back(new ASM(
        ASM::VADD,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::S2 : ftemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      asms.push_back(new ASM(
          ASM::VSTR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  }
}

void ASMParser::parseArg(vector<ASM *> &asms, IR *ir) {
  bool flag = false;
  int iCnt = 0, fCnt = 0;
  for (int i = 0; i < ir->items[2]->iVal; i++) {
    if (!ir->items[1]->symbol->params[i]->dimensions.empty() ||
        ir->items[1]->symbol->params[i]->dataType == Symbol::INT)
      iCnt++;
    else
      fCnt++;
  }
  if (ir->items[0]->type == IRItem::ITEMP) {
    if (iCnt < 4)
      return;
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
      flag = true;
    if (flag) {
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[0]->iVal])}));
      asms.push_back(
          new ASM(ASM::STR,
                  {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                   new ASMItem(((max(iCnt - 4, 0) + max(fCnt - 16, 0)) * 4))}));
    } else
      asms.push_back(new ASM(
          ASM::STR,
          {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(ASMItem::SP),
           new ASMItem(((max(iCnt - 4, 0) + max(fCnt - 16, 0)) * 4))}));
  } else {
    if (fCnt < 16)
      return;
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
      flag = true;
    if (flag) {
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[0]->iVal])}));
      asms.push_back(
          new ASM(ASM::VSTR,
                  {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                   new ASMItem(((max(iCnt - 4, 0) + max(fCnt - 16, 0)) * 4))}));
    } else
      asms.push_back(new ASM(
          ASM::VSTR,
          {new ASMItem(ftemp2Reg[ir->items[0]->iVal]), new ASMItem(ASMItem::SP),
           new ASMItem(((max(iCnt - 4, 0) + max(fCnt - 16, 0)) * 4))}));
  }
}

void ASMParser::parseB(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP) {
    asms.push_back(
        new ASM(ASM::CMP, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                           ir->items[2]->type == IRItem::INT
                               ? new ASMItem(ir->items[2]->iVal)
                               : new ASMItem(itemp2Reg[ir->items[2]->iVal])}));
    switch (ir->type) {
    case IR::BEQ:
      asms.push_back(
          new ASM(ASM::B, ASM::EQ, {new ASMItem(irLabels[ir->items[0]->ir])}));
      break;
    case IR::BGE:
      asms.push_back(
          new ASM(ASM::B, ASM::GE, {new ASMItem(irLabels[ir->items[0]->ir])}));
      break;
    case IR::BGT:
      asms.push_back(
          new ASM(ASM::B, ASM::GT, {new ASMItem(irLabels[ir->items[0]->ir])}));
      break;
    case IR::BLE:
      asms.push_back(
          new ASM(ASM::B, ASM::LE, {new ASMItem(irLabels[ir->items[0]->ir])}));
      break;
    case IR::BLT:
      asms.push_back(
          new ASM(ASM::B, ASM::LT, {new ASMItem(irLabels[ir->items[0]->ir])}));
      break;
    case IR::BNE:
      asms.push_back(
          new ASM(ASM::B, ASM::NE, {new ASMItem(irLabels[ir->items[0]->ir])}));
      break;
    default:
      break;
    }
  } else {
    if (ir->items[2]->type == IRItem::FLOAT) {
      asms.push_back(new ASM(
          ASM::MOV, {new ASMItem(ASMItem::A1),
                     new ASMItem((unsigned)ir->items[1]->iVal % ZERO16)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(ASMItem::A1),
                      new ASMItem((unsigned)ir->items[1]->iVal / ZERO16)}));
      asms.push_back(new ASM(
          ASM::VMOV, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::A1)}));
      asms.push_back(
          new ASM(ASM::VCMP, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                              new ASMItem(ASMItem::S0)}));
    } else {
      asms.push_back(
          new ASM(ASM::VCMP, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                              new ASMItem(ftemp2Reg[ir->items[2]->iVal])}));
    }
    asms.push_back(new ASM(ASM::VMRS, {}));
    asms.push_back(
        new ASM(ASM::B, ASM::EQ, {new ASMItem(irLabels[ir->items[0]->ir])}));
  }
}

void ASMParser::parseCmp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = false, flag2 = false, flag3 = false;
  if (ir->items[1]->type == IRItem::ITEMP) {
    if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end()) {
      flag2 = true;
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    }
    if (itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end()) {
      flag3 = true;
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    }
    asms.push_back(new ASM(
        ASM::CMP,
        {new ASMItem(flag2 ? ASMItem::A1 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::A2 : itemp2Reg[ir->items[2]->iVal])}));
  } else {
    if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end()) {
      flag2 = true;
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    }
    if (ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end()) {
      flag3 = true;
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S1), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    }
    asms.push_back(new ASM(
        ASM::VCMP,
        {new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::S1 : ftemp2Reg[ir->items[2]->iVal])}));
    asms.push_back(new ASM(ASM::VMRS, {}));
  }
  if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
    flag1 = true;
  switch (ir->type) {
  case IR::EQ:
    asms.push_back(new ASM(
        ASM::MOV, ASM::EQ,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(1)}));
    asms.push_back(new ASM(
        ASM::MOV, ASM::NE,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(0)}));
    break;
  case IR::GE:
    asms.push_back(new ASM(
        ASM::MOV, ASM::GE,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(1)}));
    asms.push_back(new ASM(
        ASM::MOV, ASM::LT,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(0)}));
    break;
  case IR::GT:
    asms.push_back(new ASM(
        ASM::MOV, ASM::GT,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(1)}));
    asms.push_back(new ASM(
        ASM::MOV, ASM::LE,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(0)}));
    break;
  case IR::LE:
    asms.push_back(new ASM(
        ASM::MOV, ASM::LE,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(1)}));
    asms.push_back(new ASM(
        ASM::MOV, ASM::GT,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(0)}));
    break;
  case IR::LT:
    asms.push_back(new ASM(
        ASM::MOV, ASM::LT,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(1)}));
    asms.push_back(new ASM(
        ASM::MOV, ASM::GE,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(0)}));
    break;
  case IR::NE:
    asms.push_back(new ASM(
        ASM::MOV, ASM::NE,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(1)}));
    asms.push_back(new ASM(
        ASM::MOV, ASM::EQ,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(0)}));
    break;
  default:
    break;
  }
  if (flag1) {
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                           new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  }
}

void ASMParser::parseDiv(vector<ASM *> &asms, IR *ir) {
  if (ir->items[0]->type == IRItem::ITEMP) {
    if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end())
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    else
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
    if (itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end())
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[2]->iVal])}));
    else
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A2),
                             new ASMItem(itemp2Reg[ir->items[2]->iVal])}));
    asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_idiv")}));
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[0]->iVal])}));
    else
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(ASMItem::A1)}));
  } else {
    bool flag1 = false, flag2 = false, flag3 = false;
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
      flag1 = true;
    if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end()) {
      flag2 = true;
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S1), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    }
    if (ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end()) {
      flag3 = true;
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S2), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[2]->iVal])}));
    }
    asms.push_back(new ASM(
        ASM::VDIV,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::S2 : ftemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      asms.push_back(new ASM(
          ASM::VSTR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  }
}

void ASMParser::parseF2I(vector<ASM *> &asms, IR *ir) {
  bool flag1 = false, flag2 = false;
  if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
    flag1 = true;
  if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end()) {
    flag2 = true;
    asms.push_back(
        new ASM(ASM::VLDR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                            new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  }
  asms.push_back(new ASM(
      ASM::VCVTFS,
      {new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal]),
       new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal])}));
  asms.push_back(new ASM(
      ASM::VMOV,
      {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
       new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal])}));
  if (flag2)
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                           new ASMItem(spillOffsets[ir->items[0]->iVal])}));
}

vector<ASM *> ASMParser::parseFunc(Symbol *symbol, const vector<IR *> &irs) {
  const vector<ASMItem::RegType> iRegs = {ASMItem::A1, ASMItem::A2, ASMItem::A3,
                                          ASMItem::A4};
  const vector<ASMItem::RegType> fRegs = {
      ASMItem::S0,  ASMItem::S1,  ASMItem::S2,  ASMItem::S3,
      ASMItem::S4,  ASMItem::S5,  ASMItem::S6,  ASMItem::S7,
      ASMItem::S8,  ASMItem::S9,  ASMItem::S10, ASMItem::S11,
      ASMItem::S12, ASMItem::S13, ASMItem::S14, ASMItem::S15};
  pair<int, int> regUsed = allocReg(irs);
  int argsSize = calcArgs(irs);
  for (unordered_map<unsigned, int>::iterator it = temp2SpillReg.begin();
       it != temp2SpillReg.end(); it++)
    spillOffsets[it->first] = argsSize + it->second * 4;
  vector<ASM *> asms;
  const vector<ASMItem::RegType> vRegs = {ASMItem::V1, ASMItem::V2, ASMItem::V3,
                                          ASMItem::V4, ASMItem::V5, ASMItem::V6,
                                          ASMItem::V7, ASMItem::V8};
  const vector<ASMItem::RegType> sRegs = {
      ASMItem::S16, ASMItem::S17, ASMItem::S18, ASMItem::S19,
      ASMItem::S20, ASMItem::S21, ASMItem::S22, ASMItem::S23,
      ASMItem::S24, ASMItem::S25, ASMItem::S26, ASMItem::S27,
      ASMItem::S28, ASMItem::S29, ASMItem::S30, ASMItem::S31};
  ASM *tempASM = new ASM(ASM::PUSH, {});
  for (int i = 0; i < regUsed.first; i++)
    tempASM->items.push_back(new ASMItem(vRegs[i]));
  tempASM->items.push_back(new ASMItem(ASMItem::LR));
  asms.push_back(tempASM);
  tempASM = new ASM(ASM::VPUSH, {});
  for (int i = 0; i < regUsed.second; i++)
    tempASM->items.push_back(new ASMItem(sRegs[i]));
  if (tempASM->items.empty())
    delete tempASM;
  else
    asms.push_back(tempASM);
  offsets.clear();
  int offset = 0;
  int iCnt = 0, fCnt = 0;
  int savedRegSize = 0;
  for (unsigned i = 0; i < symbol->params.size(); i++) {
    if (!symbol->params[i]->dimensions.empty() ||
        symbol->params[i]->dataType == Symbol::INT) {
      if (iCnt < 4) {
        asms.push_back(new ASM(ASM::PUSH, {new ASMItem(iRegs[iCnt++])}));
        offsets[symbol->params[i]] = -4 * (min(iCnt, 4) + min(fCnt, 16));
        savedRegSize += 4;
      } else {
        offsets[symbol->params[i]] =
            offset + (regUsed.first + regUsed.second + 1) * 4;
        offset += 4;
      }
    } else {
      if (fCnt < 16) {
        asms.push_back(new ASM(ASM::VPUSH, {new ASMItem(fRegs[fCnt++])}));
        offsets[symbol->params[i]] = -4 * (min(iCnt, 4) + min(fCnt, 16));
        savedRegSize += 4;
      } else {
        offsets[symbol->params[i]] =
            offset + (regUsed.first + regUsed.second + 1) * 4;
        offset += 4;
      }
    }
  }
  vector<Symbol *> &localVarSymbols = localVars[symbol];
  int localVarSize = 0;
  for (Symbol *localVarSymbol : localVarSymbols) {
    if (localVarSymbol->dimensions.empty())
      localVarSize += 4;
    else {
      int size = 1;
      for (int dimension : localVarSymbol->dimensions)
        size *= dimension;
      localVarSize += size * 4;
    }
    offsets[localVarSymbol] = -localVarSize - savedRegSize;
  }
  int frameSize = argsSize + spillSize + localVarSize + savedRegSize;
  if ((frameSize + (regUsed.first + regUsed.second) * 4) % 8 == 0)
    frameSize += 4;
  for (unordered_map<Symbol *, int>::iterator it = offsets.begin();
       it != offsets.end(); it++)
    it->second += frameSize;
  offset = frameSize - savedRegSize;
  if (offset <= MAX_IMM8 * 3) {
    for (int i = 0; i < offset / MAX_IMM8; i++)
      asms.push_back(
          new ASM(ASM::SUB, {new ASMItem(ASMItem::SP), new ASMItem(ASMItem::SP),
                             new ASMItem(MAX_IMM8)}));
    asms.push_back(
        new ASM(ASM::SUB, {new ASMItem(ASMItem::SP), new ASMItem(ASMItem::SP),
                           new ASMItem(offset % MAX_IMM8)}));
  } else {
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(ASMItem::A1), new ASMItem(offset % ZERO16)}));
    if (offset > MAX_IMM16)
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(ASMItem::A1), new ASMItem(offset / ZERO16)}));
    asms.push_back(
        new ASM(ASM::SUB, {new ASMItem(ASMItem::SP), new ASMItem(ASMItem::SP),
                           new ASMItem(ASMItem::A1)}));
  }
  for (unsigned i = 0; i < irs.size(); i++) {
    irId = i;
    asms.push_back(new ASM(ASM::LABEL, {new ASMItem(irLabels[irs[i]])}));
    switch (irs[i]->type) {
    case IR::ADD:
      parseAdd(asms, irs[i]);
      break;
    case IR::ARG:
      parseArg(asms, irs[i]);
      break;
    case IR::BEQ:
    case IR::BGE:
    case IR::BGT:
    case IR::BLE:
    case IR::BLT:
    case IR::BNE:
      parseB(asms, irs[i]);
      break;
    case IR::CALL:
      iCnt = 0;
      fCnt = 0;
      for (unsigned j = 1; j < irs[i]->items.size(); j++) {
        if (irs[i]->items[j]->type == IRItem::ITEMP) {
          asms.push_back(new ASM(
              ASM::MOV, {new ASMItem(iRegs[iCnt++]),
                         new ASMItem(itemp2Reg[irs[i]->items[j]->iVal])}));
        } else {
          asms.push_back(new ASM(
              ASM::VMOV, {new ASMItem(fRegs[fCnt++]),
                          new ASMItem(ftemp2Reg[irs[i]->items[j]->iVal])}));
        }
      }
      if (!irs[i]->items[0]->symbol->name.compare("_sysy_starttime")) {
        asms.push_back(new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                                          new ASMItem(startLineno % ZERO16)}));
        if (startLineno >= ZERO16)
          asms.push_back(
              new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                                  new ASMItem(startLineno / ZERO16)}));
      }
      if (!irs[i]->items[0]->symbol->name.compare("_sysy_stoptime")) {
        asms.push_back(new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                                          new ASMItem(stopLineno % ZERO16)}));
        if (startLineno >= ZERO16)
          asms.push_back(
              new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                                  new ASMItem(stopLineno / ZERO16)}));
      }
      asms.push_back(
          new ASM(ASM::BL, {new ASMItem(irs[i]->items[0]->symbol->name)}));
      break;
    case IR::DIV:
      parseDiv(asms, irs[i]);
      break;
    case IR::EQ:
    case IR::GE:
    case IR::GT:
    case IR::LE:
    case IR::LT:
    case IR::NE:
      parseCmp(asms, irs[i]);
      break;
    case IR::F2I:
      parseF2I(asms, irs[i]);
      break;
    case IR::FUNC_END:
      if (frameSize <= MAX_IMM8 * 3) {
        for (int i = 0; i < frameSize / MAX_IMM8; i++)
          asms.push_back(new ASM(ASM::ADD, {new ASMItem(ASMItem::SP),
                                            new ASMItem(ASMItem::SP),
                                            new ASMItem(MAX_IMM8)}));
        asms.push_back(new ASM(ASM::ADD, {new ASMItem(ASMItem::SP),
                                          new ASMItem(ASMItem::SP),
                                          new ASMItem(frameSize % MAX_IMM8)}));
      } else {
        asms.push_back(new ASM(ASM::MOV, {new ASMItem(ASMItem::A2),
                                          new ASMItem(frameSize % ZERO16)}));
        if (frameSize > MAX_IMM16)
          asms.push_back(new ASM(ASM::MOVT, {new ASMItem(ASMItem::A2),
                                             new ASMItem(frameSize / ZERO16)}));
        asms.push_back(new ASM(ASM::ADD, {new ASMItem(ASMItem::SP),
                                          new ASMItem(ASMItem::SP),
                                          new ASMItem(ASMItem::A2)}));
      }
      tempASM = new ASM(ASM::VPOP, {});
      for (int i = 0; i < regUsed.second; i++)
        tempASM->items.push_back(new ASMItem(sRegs[i]));
      if (tempASM->items.empty())
        delete tempASM;
      else
        asms.push_back(tempASM);
      tempASM = new ASM(ASM::POP, {});
      for (int i = 0; i < regUsed.first; i++)
        tempASM->items.push_back(new ASMItem(vRegs[i]));
      tempASM->items.push_back(new ASMItem(ASMItem::PC));
      asms.push_back(tempASM);
      break;
    case IR::GOTO:
      asms.push_back(
          new ASM(ASM::B, {new ASMItem(irLabels[irs[i]->items[0]->ir])}));
      break;
    case IR::I2F:
      parseI2F(asms, irs[i]);
      break;
    case IR::L_NOT:
      parseLNot(asms, irs[i]);
      break;
    case IR::LOAD:
      parseLoad(asms, irs[i]);
      break;
    case IR::MEMSET_ZERO: {
      if (offsets[irs[i]->items[0]->symbol] <= MAX_IMM8) {
        asms.push_back(new ASM(
            ASM::ADD, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                       new ASMItem(offsets[irs[i]->items[0]->symbol])}));
      } else if (offsets[irs[i]->items[0]->symbol] <= MAX_IMM8 * 3) {
        asms.push_back(new ASM(ASM::ADD, {new ASMItem(ASMItem::A1),
                                          new ASMItem(ASMItem::SP),
                                          new ASMItem(MAX_IMM8)}));
        for (int j = 1; j < offsets[irs[i]->items[0]->symbol] / MAX_IMM8; j++)
          asms.push_back(new ASM(ASM::ADD, {new ASMItem(ASMItem::A1),
                                            new ASMItem(ASMItem::A1),
                                            new ASMItem(MAX_IMM8)}));
        asms.push_back(new ASM(
            ASM::ADD,
            {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A1),
             new ASMItem(offsets[irs[i]->items[0]->symbol] % MAX_IMM8)}));
      } else {
        asms.push_back(
            new ASM(ASM::MOV,
                    {new ASMItem(ASMItem::A2),
                     new ASMItem(offsets[irs[i]->items[0]->symbol] % ZERO16)}));
        if (offsets[irs[i]->items[0]->symbol] > MAX_IMM16)
          asms.push_back(new ASM(
              ASM::MOVT,
              {new ASMItem(ASMItem::A2),
               new ASMItem(offsets[irs[i]->items[0]->symbol] / ZERO16)}));
        asms.push_back(new ASM(ASM::ADD, {new ASMItem(ASMItem::A1),
                                          new ASMItem(ASMItem::SP),
                                          new ASMItem(ASMItem::A2)}));
      }
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A2), new ASMItem(0)}));
      int size = 4;
      for (int dimension : irs[i]->items[0]->symbol->dimensions)
        size *= dimension;
      if (size > MAX_IMM16) {
        asms.push_back(new ASM(
            ASM::MOV, {new ASMItem(ASMItem::A3), new ASMItem(MAX_IMM16)}));
        for (int j = 1; j < (size - MAX_IMM16) / MAX_IMM8; j++)
          asms.push_back(new ASM(ASM::ADD, {new ASMItem(ASMItem::A3),
                                            new ASMItem(ASMItem::A3),
                                            new ASMItem(MAX_IMM8)}));
        asms.push_back(new ASM(
            ASM::ADD, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::A3),
                       new ASMItem((size - MAX_IMM16) % MAX_IMM8)}));
      } else {
        asms.push_back(
            new ASM(ASM::MOV, {new ASMItem(ASMItem::A3), new ASMItem(size)}));
      }
      asms.push_back(new ASM(ASM::BL, {new ASMItem("memset")}));
      break;
    }
    case IR::MOD:
      parseMod(asms, irs[i]);
      break;
    case IR::MOV:
      parseMov(asms, irs[i]);
      break;
    case IR::MUL:
      parseMul(asms, irs[i]);
      break;
    case IR::NEG:
      parseNeg(asms, irs[i]);
      break;
    case IR::RETURN:
      if (!irs[i]->items.empty()) {
        if (irs[i]->items[0]->type == IRItem::ITEMP)
          asms.push_back(new ASM(
              ASM::MOV, {new ASMItem(ASMItem::A1),
                         new ASMItem(itemp2Reg[irs[i]->items[0]->iVal])}));
        else
          asms.push_back(new ASM(
              ASM::VMOV, {new ASMItem(ASMItem::S0),
                          new ASMItem(ftemp2Reg[irs[i]->items[0]->iVal])}));
      }
      asms.push_back(new ASM(ASM::B, {new ASMItem(irLabels[irs.back()])}));
      break;
    case IR::STORE:
      if (irs[i]->items[1]->type == IRItem::ITEMP)
        asms.push_back(new ASM(
            ASM::STR, {new ASMItem(itemp2Reg[irs[i]->items[1]->iVal]),
                       new ASMItem(itemp2Reg[irs[i]->items[0]->iVal])}));
      else
        asms.push_back(new ASM(
            ASM::VSTR, {new ASMItem(ftemp2Reg[irs[i]->items[1]->iVal]),
                        new ASMItem(itemp2Reg[irs[i]->items[0]->iVal])}));
      break;
    case IR::SUB:
      parseSub(asms, irs[i]);
      break;
    default:
      break;
    }
  }
  return asms;
}

void ASMParser::parseI2F(vector<ASM *> &asms, IR *ir) {
  bool flag1 = false, flag2 = false;
  if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
    flag1 = true;
  if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end()) {
    flag2 = true;
    asms.push_back(
        new ASM(ASM::LDR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                           new ASMItem(spillOffsets[ir->items[1]->iVal])}));
  }
  asms.push_back(new ASM(
      ASM::VMOV,
      {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
       new ASMItem(flag2 ? ASMItem::A1 : itemp2Reg[ir->items[1]->iVal])}));
  asms.push_back(new ASM(
      ASM::VCVTSF,
      {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
       new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal])}));
  if (flag2)
    asms.push_back(
        new ASM(ASM::VSTR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                            new ASMItem(spillOffsets[ir->items[0]->iVal])}));
}

void ASMParser::parseLNot(vector<ASM *> &asms, IR *ir) {
  bool flag1 = false, flag2 = false;
  if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
    flag1 = true;
  if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end())
    flag2 = true;
  if (flag1) {
    if (flag2)
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    else {
      if (ir->items[1]->type == IRItem::ITEMP)
        asms.push_back(
            new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                               new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
      else
        asms.push_back(
            new ASM(ASM::VMOV, {new ASMItem(ASMItem::A1),
                                new ASMItem(ftemp2Reg[ir->items[1]->iVal])}));
    }
    asms.push_back(
        new ASM(ASM::CMP, {new ASMItem(ASMItem::A1), new ASMItem(0)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ, {new ASMItem(ASMItem::A1), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::NE, {new ASMItem(ASMItem::A1), new ASMItem(0)}));
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                           new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  } else {
    if (flag2)
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    else {
      if (ir->items[1]->type == IRItem::ITEMP)
        asms.push_back(
            new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                               new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
      else
        asms.push_back(
            new ASM(ASM::VMOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                                new ASMItem(ftemp2Reg[ir->items[1]->iVal])}));
    }
    asms.push_back(
        new ASM(ASM::CMP,
                {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(0)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ,
                {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::NE,
                {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(0)}));
  }
}

void ASMParser::parseLoad(vector<ASM *> &asms, IR *ir) {
  bool flag1 = false, flag2 = false;
  if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end()) {
    flag1 = true;
    asms.push_back(
        new ASM(ASM::LDR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                           new ASMItem(spillOffsets[ir->items[1]->iVal])}));
  }
  if (ir->items[0]->type == IRItem::ITEMP) {
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
      flag2 = true;
    asms.push_back(new ASM(
        ASM::LDR,
        {new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[1]->iVal])}));
    if (flag2)
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  } else {
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
      flag2 = true;
    asms.push_back(new ASM(
        ASM::VLDR,
        {new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[1]->iVal])}));
    if (flag2)
      asms.push_back(new ASM(
          ASM::VSTR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  }
}

void ASMParser::parseMod(vector<ASM *> &asms, IR *ir) {
  if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end())
    asms.push_back(
        new ASM(ASM::LDR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                           new ASMItem(spillOffsets[ir->items[1]->iVal])}));
  else
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                           new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
  if (itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end())
    asms.push_back(
        new ASM(ASM::LDR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::SP),
                           new ASMItem(spillOffsets[ir->items[2]->iVal])}));
  else
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(ASMItem::A2),
                           new ASMItem(itemp2Reg[ir->items[2]->iVal])}));
  asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_idivmod")}));
  if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                           new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  else
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                           new ASMItem(ASMItem::A2)}));
}

void ASMParser::parseMov(vector<ASM *> &asms, IR *ir) {
  bool flag1 = false, flag2 = false;
  switch (ir->items[1]->type) {
  case IRItem::FLOAT:
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(ASMItem::A1),
                   new ASMItem((unsigned)ir->items[1]->iVal % ZERO16)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(ASMItem::A1),
                    new ASMItem((unsigned)ir->items[1]->iVal / ZERO16)}));
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[0]->iVal])}));
    else
      asms.push_back(
          new ASM(ASM::VMOV, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                              new ASMItem(ASMItem::A1)}));
    break;
  case IRItem::FTEMP:
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
      flag1 = true;
    if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end())
      flag2 = true;
    if (flag1) {
      if (flag2) {
        asms.push_back(new ASM(
            ASM::VLDR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                        new ASMItem(spillOffsets[ir->items[1]->iVal])}));
        asms.push_back(new ASM(
            ASM::VSTR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                        new ASMItem(spillOffsets[ir->items[0]->iVal])}));
      } else
        asms.push_back(new ASM(
            ASM::VSTR, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                        new ASMItem(ASMItem::SP),
                        new ASMItem(spillOffsets[ir->items[0]->iVal])}));
    } else {
      if (flag2)
        asms.push_back(new ASM(
            ASM::VSTR, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                        new ASMItem(ASMItem::SP),
                        new ASMItem(spillOffsets[ir->items[0]->iVal])}));
      else
        asms.push_back(
            new ASM(ASM::VMOV, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                                new ASMItem(ftemp2Reg[ir->items[1]->iVal])}));
    }
    break;
  case IRItem::INT:
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
      flag1 = true;
    if (ir->items[1]->iVal < 0 || ir->items[1]->iVal > MAX_IMM16) {
      asms.push_back(new ASM(
          ASM::MOV,
          {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem((unsigned)ir->items[1]->iVal % ZERO16)}));
      asms.push_back(new ASM(
          ASM::MOVT,
          {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem((unsigned)ir->items[1]->iVal / ZERO16)}));
    } else
      asms.push_back(new ASM(
          ASM::MOV,
          {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(ir->items[1]->iVal)}));
    if (flag1)
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[0]->iVal])}));
    break;
  case IRItem::ITEMP:
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
      flag1 = true;
    if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end())
      flag2 = true;
    if (flag1) {
      if (flag2) {
        asms.push_back(new ASM(
            ASM::VLDR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                        new ASMItem(spillOffsets[ir->items[1]->iVal])}));
        asms.push_back(new ASM(
            ASM::VSTR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                        new ASMItem(spillOffsets[ir->items[0]->iVal])}));
      } else
        asms.push_back(
            new ASM(ASM::STR, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                               new ASMItem(ASMItem::SP),
                               new ASMItem(spillOffsets[ir->items[0]->iVal])}));
    } else {
      if (flag2)
        asms.push_back(
            new ASM(ASM::STR, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                               new ASMItem(ASMItem::SP),
                               new ASMItem(spillOffsets[ir->items[0]->iVal])}));
      else
        asms.push_back(
            new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                               new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
    }
    break;
  case IRItem::RETURN:
    if (ir->items[0]->type == IRItem::ITEMP) {
      if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
        asms.push_back(new ASM(
            ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                       new ASMItem(spillOffsets[ir->items[0]->iVal])}));
      else
        asms.push_back(
            new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                               new ASMItem(ASMItem::A1)}));
    } else {
      if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
        asms.push_back(new ASM(
            ASM::STR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                       new ASMItem(spillOffsets[ir->items[0]->iVal])}));
      else
        asms.push_back(
            new ASM(ASM::VMOV, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                                new ASMItem(ASMItem::S0)}));
    }
    break;
  case IRItem::SYMBOL:
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
      flag1 = true;
    switch (ir->items[1]->symbol->symbolType) {
    case Symbol::CONST:
    case Symbol::GLOBAL_VAR:
      asms.push_back(new ASM(
          ASM::LDR,
          {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem("l" + to_string(labelId))}));
      asms.push_back(
          new ASM(ASM::LABEL, {new ASMItem("l" + to_string(labelId + 1))}));
      asms.push_back(new ASM(
          ASM::ADD,
          {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(ASMItem::PC),
           new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal])}));
      asms.push_back(
          new ASM(ASM::B, {new ASMItem("l" + to_string(labelId + 2))}));
      asms.push_back(
          new ASM(ASM::LABEL, {new ASMItem("l" + to_string(labelId++))}));
      asms.push_back(
          new ASM(ASM::TAG, {new ASMItem(ir->items[1]->symbol->name),
                             new ASMItem("l" + to_string(labelId++))}));
      asms.push_back(
          new ASM(ASM::LABEL, {new ASMItem("l" + to_string(labelId++))}));
      if (flag1)
        asms.push_back(new ASM(
            ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                       new ASMItem(spillOffsets[ir->items[0]->iVal])}));
      break;
    case Symbol::LOCAL_VAR:
    case Symbol::PARAM:
      if (offsets[ir->items[1]->symbol] > MAX_IMM8) {
        if (offsets[ir->items[1]->symbol] <= MAX_IMM8 * 3) {
          asms.push_back(new ASM(
              ASM::ADD,
              {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(ASMItem::SP), new ASMItem(MAX_IMM8)}));
          for (int j = 1; j < offsets[ir->items[1]->symbol] / MAX_IMM8; j++)
            asms.push_back(new ASM(
                ASM::ADD, {new ASMItem(flag1 ? ASMItem::A1
                                             : itemp2Reg[ir->items[0]->iVal]),
                           new ASMItem(flag1 ? ASMItem::A1
                                             : itemp2Reg[ir->items[0]->iVal]),
                           new ASMItem(MAX_IMM8)}));
          asms.push_back(new ASM(
              ASM::ADD,
              {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(offsets[ir->items[1]->symbol] % MAX_IMM8)}));
        } else {
          asms.push_back(new ASM(
              ASM::MOV,
              {new ASMItem(ASMItem::A1),
               new ASMItem((unsigned)offsets[ir->items[1]->symbol] % ZERO16)}));
          if (offsets[ir->items[1]->symbol] > MAX_IMM16)
            asms.push_back(
                new ASM(ASM::MOVT,
                        {new ASMItem(ASMItem::A1),
                         new ASMItem((unsigned)offsets[ir->items[1]->symbol] /
                                     ZERO16)}));
          asms.push_back(new ASM(
              ASM::ADD,
              {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(ASMItem::SP), new ASMItem(ASMItem::A1)}));
        }
      } else {
        asms.push_back(new ASM(
            ASM::ADD,
            {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
             new ASMItem(ASMItem::SP),
             new ASMItem(offsets[ir->items[1]->symbol])}));
      }
      if (flag1)
        asms.push_back(new ASM(
            ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                       new ASMItem(spillOffsets[ir->items[0]->iVal])}));
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMul(vector<ASM *> &asms, IR *ir) {
  bool flag1 = false, flag2 = false, flag3 = false;
  if (ir->items[0]->type == IRItem::ITEMP) {
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
      flag1 = true;
    if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end()) {
      flag2 = true;
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    }
    if (itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end()) {
      flag3 = true;
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[2]->iVal])}));
    }
    asms.push_back(new ASM(
        ASM::MUL,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::A3 : itemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  } else {
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
      flag1 = true;
    if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end()) {
      flag2 = true;
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S1), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    }
    if (ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end()) {
      flag3 = true;
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S2), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[2]->iVal])}));
    }
    asms.push_back(new ASM(
        ASM::VMUL,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::S2 : ftemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      asms.push_back(new ASM(
          ASM::VSTR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  }
}

void ASMParser::parseNeg(vector<ASM *> &asms, IR *ir) {
  bool flag1 = false, flag2 = false;
  if (ir->items[0]->type == IRItem::ITEMP) {
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
      flag1 = true;
    if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end()) {
      flag2 = true;
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    }
    asms.push_back(new ASM(
        ASM::RSB,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(0)}));
    if (flag1)
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  } else {
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
      flag1 = true;
    if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end()) {
      flag2 = true;
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S1), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    }
    asms.push_back(new ASM(
        ASM::VNEG,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal])}));
    if (flag1)
      asms.push_back(new ASM(
          ASM::VSTR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  }
}

void ASMParser::parseSub(vector<ASM *> &asms, IR *ir) {
  bool flag1 = false, flag2 = false, flag3 = false;
  if (ir->items[0]->type == IRItem::ITEMP) {
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
      flag1 = true;
    if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end()) {
      flag2 = true;
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    }
    if (itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end()) {
      flag3 = true;
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[2]->iVal])}));
    }
    asms.push_back(new ASM(
        ASM::SUB,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::A3 : itemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  } else {
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
      flag1 = true;
    if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end()) {
      flag2 = true;
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S1), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    }
    if (ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end()) {
      flag3 = true;
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S2), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[2]->iVal])}));
    }
    asms.push_back(new ASM(
        ASM::VSUB,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::S2 : ftemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      asms.push_back(new ASM(
          ASM::VSTR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  }
}

void ASMParser::preProcess() {
  int id = 0;
  for (Symbol *symbol : consts)
    symbol->name = "var" + to_string(id++);
  for (Symbol *symbol : globalVars)
    symbol->name = "var" + to_string(id++);
  id = 0;
  for (pair<Symbol *, vector<IR *>> &funcIR : funcIRs) {
    if (funcIR.first->name.compare("main"))
      funcIR.first->name = "f" + to_string(id++);
    vector<string> labels;
    for (IR *ir : funcIR.second)
      irLabels[ir] = "l" + to_string(labelId++);
  }
}

void ASMParser::removeUnusedLabels(vector<ASM *> &asms) {
  unordered_set<string> usedLabels;
  for (ASM *a : asms)
    if (a->type != ASM::LABEL)
      for (ASMItem *item : a->items)
        if (item->type == ASMItem::LABEL)
          usedLabels.insert(item->sVal);
  int size = 0;
  for (ASM *a : asms)
    if (a->type == ASM::LABEL &&
        usedLabels.find(a->items[0]->sVal) == usedLabels.end())
      delete a;
    else
      asms[size++] = a;
  asms.resize(size);
}

void ASMParser::writeASMFile() {
  parse();
  FILE *file = fopen(asmFile.data(), "w");
  fprintf(file, "\t.arch armv7-a\n");
  fprintf(file, "\t.fpu vfpv4\n");
  fprintf(file, "\t.arm\n");
  fprintf(file, "\t.data\n");
  for (Symbol *symbol : consts) {
    int size = 4;
    for (int dimension : symbol->dimensions)
      size *= dimension;
    fprintf(file, "\t.global %s\n", symbol->name.data());
    fprintf(file, "\t.size %s, %d\n", symbol->name.data(), size);
    fprintf(file, "%s:\n", symbol->name.data());
    if (symbol->dimensions.empty())
      fprintf(file, "\t.word %d\n", symbol->iVal);
    else
      for (int i = 0; i < size / 4; i++)
        fprintf(file, "\t.word %d\n", symbol->iMap[i]);
  }
  for (Symbol *symbol : globalVars) {
    int size = 4;
    for (int dimension : symbol->dimensions)
      size *= dimension;
    fprintf(file, "\t.size %s, %d\n", symbol->name.data(), size);
    fprintf(file, "%s:\n", symbol->name.data());
    if (symbol->dimensions.empty())
      fprintf(file, "\t.word %d\n", symbol->iVal);
    else {
      if (symbol->iMap.empty())
        fprintf(file, "\t.space %d\n", size);
      else
        for (int i = 0; i < size / 4; i++)
          fprintf(file, "\t.word %d\n", symbol->iMap[i]);
    }
  }
  fprintf(file, "\t.text\n");
  for (pair<Symbol *, vector<ASM *>> &funcASM : funcASMs) {
    fprintf(file, "\t.global %s\n", funcASM.first->name.data());
    fprintf(file, "%s:\n", funcASM.first->name.data());
    for (unsigned i = 0; i < funcASM.second.size(); i++)
      fprintf(file, "%s\n", funcASM.second[i]->toString().data());
  }
  fprintf(file, "\t.section .note.GNU-stack,\"\",%%progbits\n");
  fclose(file);
}