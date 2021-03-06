#include <algorithm>
#include <iostream>

#include "ASMParser.h"
#include "RegAllocator.h"

using namespace std;

ASMParser::ASMParser(pair<unsigned, unsigned> lineno,
                     vector<pair<Symbol *, vector<IR *>>> &funcIRs,
                     vector<Symbol *> &consts, vector<Symbol *> &globalVars,
                     unordered_map<Symbol *, vector<Symbol *>> &localVars) {
  this->isProcessed = false;
  this->labelId = 0;
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

int ASMParser::calcCallArgSize(const vector<IR *> &irs) {
  int size = 0;
  for (IR *ir : irs)
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
  return size * 4;
}

vector<pair<Symbol *, vector<ASM *>>> ASMParser::getFuncASMs() {
  if (!isProcessed)
    parse();
  return funcASMs;
}

void ASMParser::loadFromSP(vector<ASM *> &asms, ASMItem::RegType target,
                           unsigned offset) {
  vector<unsigned> smartBytes;
  for (int i = 30; i >= 0; i -= 2) {
    if (offset & (0x3 << i)) {
      if (i >= 6)
        smartBytes.push_back(offset & (0xff << (i - 6)));
      else
        smartBytes.push_back(offset & (0xff >> (6 - i)));
      i -= 6;
    }
  }
  ASM::ASMOpType op = isFloatReg(target) ? ASM::VLDR : ASM::LDR;
  unsigned maxOffset = isFloatReg(target) ? 1020 : 4095;
  switch (smartBytes.size()) {
  case 0:
    asms.push_back(
        new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::SP)}));
    break;
  case 1:
    if (offset <= maxOffset)
      asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::SP),
                                  new ASMItem(offset)}));
    else {
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::SP),
                             new ASMItem(offset)}));
      asms.push_back(
          new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::A4)}));
    }
    break;
  case 2:
    if (offset <= maxOffset) {
      asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::SP),
                                  new ASMItem(offset)}));
    } else if (!(offset & 0xffff0000)) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(offset)}));
      asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::SP),
                                  new ASMItem(ASMItem::A4)}));
    } else if (smartBytes[1] <= maxOffset) {
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::SP),
                             new ASMItem(smartBytes[0])}));
      asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::A4),
                                  new ASMItem(smartBytes[1])}));
    } else {
      asms.push_back(new ASM(
          ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(offset & 0xffff)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(ASMItem::A4), new ASMItem(offset >> 16)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::SP),
                             new ASMItem(ASMItem::A4)}));
      asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::SP),
                                  new ASMItem(ASMItem::A4)}));
    }
    break;
  default:
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(offset & 0xffff)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(ASMItem::A4), new ASMItem(offset >> 16)}));
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::SP),
                           new ASMItem(ASMItem::A4)}));
    asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::SP),
                                new ASMItem(ASMItem::A4)}));
    break;
  }
}

void ASMParser::loadImmToReg(vector<ASM *> &asms, ASMItem::RegType reg,
                             float val) {
  union {
    float fVal;
    unsigned iVal;
  } unionData;
  unionData.fVal = val;
  unsigned data = unionData.iVal;
  int exp = (data >> 23) & 0xff;
  if (exp >= 124 && exp <= 131 && !(data & 0x7ffff)) {
    asms.push_back(new ASM(ASM::VMOV, {new ASMItem(reg), new ASMItem(data)}));
    return;
  }
  asms.push_back(new ASM(
      ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(data & 0xffff)}));
  if (data & 0xffff0000)
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(ASMItem::A4), new ASMItem(data >> 16)}));
  asms.push_back(
      new ASM(ASM::VMOV, {new ASMItem(reg), new ASMItem(ASMItem::A4)}));
}

void ASMParser::loadImmToReg(vector<ASM *> &asms, ASMItem::RegType reg,
                             int val) {
  asms.push_back(
      new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(val & 0xffff)}));
  if (val & 0xffff0000)
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(reg), new ASMItem(((unsigned)val) >> 16)}));
}

void ASMParser::initFrame() {
  savedRegs = 0;
  frameOffset = 0;
  ftemp2Reg.clear();
  itemp2Reg.clear();
  temp2SpillReg.clear();
  spillOffsets.clear();
  offsets.clear();
}

bool ASMParser::isFloatReg(ASMItem::RegType reg) { return reg >= ASMItem::S0; }

void ASMParser::makeFrame(vector<ASM *> &asms, const vector<IR *> &irs,
                          Symbol *func) {
  RegAllocator *allocator = new RegAllocator(irs);
  usedRegNum = allocator->getUsedRegNum();
  itemp2Reg = allocator->getItemp2Reg();
  ftemp2Reg = allocator->getFtemp2Reg();
  temp2SpillReg = allocator->getTemp2SpillReg();
  delete allocator;
  int callArgSize = calcCallArgSize(irs);
  for (unordered_map<unsigned, unsigned>::iterator it = temp2SpillReg.begin();
       it != temp2SpillReg.end(); it++)
    spillOffsets[it->first] = callArgSize + it->second * 4;
  saveUsedRegs(asms);
  offsets.clear();
  saveArgRegs(asms, func);
  int localVarSize = 0;
  for (Symbol *localVarSymbol : localVars[func]) {
    int size = 4;
    for (int dimension : localVarSymbol->dimensions)
      size *= dimension;
    localVarSize += size;
    offsets[localVarSymbol] = -localVarSize - savedRegs * 4;
  }
  frameOffset = callArgSize + usedRegNum[2] * 4 + localVarSize + savedRegs * 4;
  if ((frameOffset + (usedRegNum[0] + usedRegNum[1]) * 4) % 8 == 0)
    frameOffset += 4;
  for (unordered_map<Symbol *, int>::iterator it = offsets.begin();
       it != offsets.end(); it++)
    it->second += frameOffset;
  moveFromSP(asms, ASMItem::SP, (int)savedRegs * 4 - frameOffset);
}

void ASMParser::moveFromSP(vector<ASM *> &asms, ASMItem::RegType target,
                           int offset) {
  vector<unsigned> smartBytes(5);
  ASM::ASMOpType op = offset > 0 ? ASM::ADD : ASM::SUB;
  offset = abs(offset);
  for (int i = 0; i < 4; i++) {
    vector<unsigned> temps;
    for (unsigned j = 0; j < 16; j++) {
      if (offset & (0x3 << (((i + j) % 16) * 2))) {
        temps.push_back(offset &
                        ((0xff << min<unsigned>(((i + j) % 16) * 2, 32)) |
                         max(0xff >> (((i + j) % 16) * 2 - 24), 0)));
        j += 3;
      }
    }
    if (smartBytes.size() > temps.size())
      smartBytes = temps;
  }
  switch (smartBytes.size()) {
  case 0:
    if (target != ASMItem::SP)
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(target), new ASMItem(ASMItem::SP)}));
    break;
  case 1:
    asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::SP),
                                new ASMItem(smartBytes[0])}));
    break;
  case 2:
    if (!(offset & 0xffff0000)) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(offset)}));
      asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::SP),
                                  new ASMItem(ASMItem::A4)}));
    } else {
      asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::SP),
                                  new ASMItem(smartBytes[0])}));
      asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(target),
                                  new ASMItem(smartBytes[1])}));
    }
    break;
  default:
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(offset & 0xffff)}));
    asms.push_back(new ASM(ASM::MOVT, {new ASMItem(ASMItem::A4),
                                       new ASMItem(((unsigned)offset) >> 16)}));
    asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::SP),
                                new ASMItem(ASMItem::A4)}));
    break;
  }
}

void ASMParser::parse() {
  isProcessed = true;
  preProcess();
  for (const pair<Symbol *, vector<IR *>> &funcIR : funcIRs) {
    vector<ASM *> asms = parseFunc(funcIR.first, funcIR.second);
    funcASMs.emplace_back(funcIR.first, asms);
  }
}

void ASMParser::parseAlgo(vector<ASM *> &asms, ASM::ASMOpType iOp,
                          ASM::ASMOpType fOp, IR *ir) {
  if (ir->items[0]->type == IRItem::ITEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
         flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadFromSP(asms, ASMItem::A3, spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        iOp,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::A3 : itemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
  } else {
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
         flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S1, spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadFromSP(asms, ASMItem::S2, spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        fOp,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::S2 : ftemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      storeFromSP(asms, ASMItem::S0, spillOffsets[ir->items[0]->iVal]);
  }
}

void ASMParser::parseB(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP) {
    if (ir->items[2]->type == IRItem::INT) {
      bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
      if (flag2)
        loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[1]->iVal]);
      loadImmToReg(asms, ASMItem::A2, 0);
      asms.push_back(new ASM(
          ASM::CMP,
          {new ASMItem(flag2 ? ASMItem::A1 : itemp2Reg[ir->items[1]->iVal]),
           new ASMItem(ASMItem::A2)}));
    } else {
      bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
           flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
      if (flag2)
        loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[1]->iVal]);
      if (flag3)
        loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[2]->iVal]);
      asms.push_back(new ASM(
          ASM::CMP,
          {new ASMItem(flag2 ? ASMItem::A1 : itemp2Reg[ir->items[1]->iVal]),
           new ASMItem(flag3 ? ASMItem::A2 : itemp2Reg[ir->items[2]->iVal])}));
    }
  } else {
    if (ir->items[2]->type == IRItem::FLOAT) {
      bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
      if (flag2)
        loadFromSP(asms, ASMItem::S0, spillOffsets[ir->items[1]->iVal]);
      loadImmToReg(asms, ASMItem::S1, 0.0f);
      asms.push_back(new ASM(
          ASM::VCMP,
          {new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal]),
           new ASMItem(ASMItem::S1)}));
      asms.push_back(new ASM(ASM::VMRS, {}));
    } else {
      bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
           flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
      if (flag2)
        loadFromSP(asms, ASMItem::S0, spillOffsets[ir->items[1]->iVal]);
      if (flag3)
        loadFromSP(asms, ASMItem::S1, spillOffsets[ir->items[2]->iVal]);
      asms.push_back(new ASM(
          ASM::VCMP,
          {new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal]),
           new ASMItem(flag3 ? ASMItem::S1 : ftemp2Reg[ir->items[2]->iVal])}));
      asms.push_back(new ASM(ASM::VMRS, {}));
    }
  }
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

void ASMParser::parseCall(vector<ASM *> &asms, IR *ir) {
  unsigned iCnt = 0, fCnt = 0, offset = 0;
  for (unsigned i = 1; i < ir->items.size(); i++) {
    if (ir->items[i]->type == IRItem::ITEMP) {
      if (iCnt < 4) {
        iCnt++;
        continue;
      }
      if (itemp2Reg.find(ir->items[i]->iVal) == itemp2Reg.end()) {
        loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[i]->iVal]);
        storeFromSP(asms, ASMItem::A1, offset);
      } else
        storeFromSP(asms, itemp2Reg[ir->items[i]->iVal], offset);
    } else {
      if (fCnt < 16) {
        fCnt++;
        continue;
      }
      if (ftemp2Reg.find(ir->items[i]->iVal) == ftemp2Reg.end()) {
        loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[i]->iVal]);
        storeFromSP(asms, ASMItem::A1, offset);
      } else
        storeFromSP(asms, ftemp2Reg[ir->items[i]->iVal], offset);
    }
    offset += 4;
  }
  iCnt = 0;
  fCnt = 0;
  for (unsigned i = 1; i < ir->items.size(); i++) {
    if (ir->items[i]->type == IRItem::ITEMP) {
      if (iCnt < 4) {
        if (itemp2Reg.find(ir->items[i]->iVal) == itemp2Reg.end())
          loadFromSP(asms, aIRegs[iCnt++], spillOffsets[ir->items[i]->iVal]);
        else
          asms.push_back(
              new ASM(ASM::MOV, {new ASMItem(aIRegs[iCnt++]),
                                 new ASMItem(itemp2Reg[ir->items[i]->iVal])}));
      }
    } else {
      if (fCnt < 16) {
        if (ftemp2Reg.find(ir->items[i]->iVal) == ftemp2Reg.end())
          loadFromSP(asms, aFRegs[fCnt++], spillOffsets[ir->items[i]->iVal]);
        else
          asms.push_back(
              new ASM(ASM::VMOV, {new ASMItem(aFRegs[fCnt++]),
                                  new ASMItem(ftemp2Reg[ir->items[i]->iVal])}));
      }
    }
  }
  if (!ir->items[0]->symbol->name.compare("_sysy_starttime"))
    loadImmToReg(asms, ASMItem::A1, startLineno);
  if (!ir->items[0]->symbol->name.compare("_sysy_stoptime"))
    loadImmToReg(asms, ASMItem::A1, stopLineno);
  asms.push_back(new ASM(ASM::BL, {new ASMItem(ir->items[0]->symbol->name)}));
}

void ASMParser::parseCmp(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP) {
    if (ir->items[2]->type == IRItem::INT) {
      bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
      if (flag2)
        loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[1]->iVal]);
      loadImmToReg(asms, ASMItem::A2, 0);
      asms.push_back(new ASM(
          ASM::CMP,
          {new ASMItem(flag2 ? ASMItem::A1 : itemp2Reg[ir->items[1]->iVal]),
           new ASMItem(ASMItem::A2)}));
    } else {
      bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
           flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
      if (flag2)
        loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[1]->iVal]);
      if (flag3)
        loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[2]->iVal]);
      asms.push_back(new ASM(
          ASM::CMP,
          {new ASMItem(flag2 ? ASMItem::A1 : itemp2Reg[ir->items[1]->iVal]),
           new ASMItem(flag3 ? ASMItem::A2 : itemp2Reg[ir->items[2]->iVal])}));
    }
  } else {
    if (ir->items[2]->type == IRItem::FLOAT) {
      bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
      if (flag2)
        loadFromSP(asms, ASMItem::S0, spillOffsets[ir->items[1]->iVal]);
      loadImmToReg(asms, ASMItem::S1, 0.0f);
      asms.push_back(new ASM(
          ASM::VCMP,
          {new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal]),
           new ASMItem(ASMItem::S1)}));
      asms.push_back(new ASM(ASM::VMRS, {}));
    } else {
      bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
           flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
      if (flag2)
        loadFromSP(asms, ASMItem::S0, spillOffsets[ir->items[1]->iVal]);
      if (flag3)
        loadFromSP(asms, ASMItem::S1, spillOffsets[ir->items[2]->iVal]);
      asms.push_back(new ASM(
          ASM::VCMP,
          {new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal]),
           new ASMItem(flag3 ? ASMItem::S1 : ftemp2Reg[ir->items[2]->iVal])}));
      asms.push_back(new ASM(ASM::VMRS, {}));
    }
  }
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end();
  ASMItem::RegType targetReg =
      flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal];
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
    storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseF2I(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, ASMItem::S0, spillOffsets[ir->items[1]->iVal]);
  asms.push_back(new ASM(
      ASM::VCVTFS,
      {new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal]),
       new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal])}));
  asms.push_back(new ASM(
      ASM::VMOV,
      {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
       new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal])}));
  if (flag1)
    storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
}

vector<ASM *> ASMParser::parseFunc(Symbol *func, const vector<IR *> &irs) {
  vector<ASM *> asms;
  initFrame();
  makeFrame(asms, irs, func);
  for (unsigned i = 0; i < irs.size(); i++) {
    switch (irs[i]->type) {
    case IR::ADD:
      parseAlgo(asms, ASM::ADD, ASM::VADD, irs[i]);
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
      parseCall(asms, irs[i]);
      break;
    case IR::DIV:
      parseAlgo(asms, ASM::SDIV, ASM::VDIV, irs[i]);
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
    case IR::GOTO:
      asms.push_back(new ASM(
          ASM::B,
          {new ASMItem(ASMItem::LABEL, irLabels[irs[i]->items[0]->ir])}));
      break;
    case IR::I2F:
      parseI2F(asms, irs[i]);
      break;
    case IR::L_NOT:
      parseLNot(asms, irs[i]);
      break;
    case IR::LABEL:
      asms.push_back(
          new ASM(ASM::LABEL, {new ASMItem(ASMItem::LABEL, irLabels[irs[i]])}));
      break;
    case IR::MEMSET_ZERO:
      parseMemsetZero(asms, irs[i]);
      break;
    case IR::MOV:
      switch (irs[i]->items[0]->type) {
      case IRItem::FTEMP:
        parseMovToFtemp(asms, irs[i]);
        break;
      case IRItem::ITEMP:
        parseMovToItemp(asms, irs[i]);
        break;
      case IRItem::SYMBOL:
        parseMovToSymbol(asms, irs[i]);
        break;
      default:
        break;
      }
      break;
    case IR::MUL:
      parseAlgo(asms, ASM::MUL, ASM::VMUL, irs[i]);
      break;
    case IR::NEG:
      parseNeg(asms, irs[i]);
      break;
    case IR::RETURN:
      parseReturn(asms, irs[i], irs.back());
      break;
    case IR::SUB:
      parseAlgo(asms, ASM::SUB, ASM::VSUB, irs[i]);
      break;
    default:
      break;
    }
  }
  moveFromSP(asms, ASMItem::SP, frameOffset);
  popArgs(asms);
  return asms;
}

void ASMParser::parseI2F(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal],
               spillOffsets[ir->items[1]->iVal]);
  else
    asms.push_back(new ASM(
        ASM::VMOV,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
  asms.push_back(new ASM(
      ASM::VCVTSF,
      {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
       new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal])}));
  if (flag1)
    storeFromSP(asms, ASMItem::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseLNot(vector<ASM *> &asms, IR *ir) {
  bool flag = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end();
  ASMItem::RegType targetReg =
      flag ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal];
  if (itemp2Reg.find(ir->items[1]->iVal) != itemp2Reg.end()) {
    asms.push_back(
        new ASM(ASM::CMP,
                {new ASMItem(itemp2Reg[ir->items[1]->iVal]), new ASMItem(0)}));
  } else {
    if (ftemp2Reg.find(ir->items[1]->iVal) != ftemp2Reg.end())
      asms.push_back(
          new ASM(ASM::VMOV, {new ASMItem(targetReg),
                              new ASMItem(ftemp2Reg[ir->items[1]->iVal])}));
    else
      loadFromSP(asms, targetReg, spillOffsets[ir->items[1]->iVal]);
    asms.push_back(new ASM(ASM::CMP, {new ASMItem(targetReg), new ASMItem(0)}));
  }
  asms.push_back(
      new ASM(ASM::MOV, ASM::EQ, {new ASMItem(targetReg), new ASMItem(1)}));
  asms.push_back(
      new ASM(ASM::MOV, ASM::NE, {new ASMItem(targetReg), new ASMItem(0)}));
  if (flag)
    storeFromSP(asms, targetReg, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseMemsetZero(vector<ASM *> &asms, IR *ir) {
  moveFromSP(asms, ASMItem::A1, offsets[ir->items[0]->symbol]);
  asms.push_back(new ASM(ASM::MOV, {new ASMItem(ASMItem::A2), new ASMItem(0)}));
  int size = 4;
  for (int dimension : ir->items[0]->symbol->dimensions)
    size *= dimension;
  loadImmToReg(asms, ASMItem::A3, size);
  asms.push_back(new ASM(ASM::BL, {new ASMItem("memset")}));
}

void ASMParser::parseMovToFtemp(vector<ASM *> &asms, IR *ir) {
  switch (ir->items[1]->type) {
  case IRItem::FLOAT:
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end()) {
      loadImmToReg(asms, ASMItem::A1, ir->items[1]->iVal);
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
    } else
      loadImmToReg(asms, ftemp2Reg[ir->items[0]->iVal], ir->items[1]->fVal);
    break;
  case IRItem::FTEMP:
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end()) {
      if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end()) {
        loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[1]->iVal]);
        storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
      } else
        storeFromSP(asms, ftemp2Reg[ir->items[1]->iVal],
                    spillOffsets[ir->items[0]->iVal]);
    } else {
      if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end())
        loadFromSP(asms, ftemp2Reg[ir->items[0]->iVal],
                   spillOffsets[ir->items[1]->iVal]);
      else
        asms.push_back(
            new ASM(ASM::VMOV, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                                new ASMItem(ftemp2Reg[ir->items[1]->iVal])}));
    }
    break;
  case IRItem::RETURN:
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
      storeFromSP(asms, ASMItem::S0, spillOffsets[ir->items[0]->iVal]);
    else
      asms.push_back(
          new ASM(ASM::VMOV, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                              new ASMItem(ASMItem::S0)}));
    break;
  case IRItem::SYMBOL: {
    vector<unsigned> sizes({4});
    for (int i = ir->items[1]->symbol->dimensions.size() - 1; i > 0; i--)
      sizes.push_back(sizes.back() * ir->items[1]->symbol->dimensions[i]);
    reverse(sizes.begin(), sizes.end());
    unsigned offset = 0;
    vector<pair<unsigned, unsigned>> temps;
    for (unsigned i = 0; i < ir->items.size() - 2; i++) {
      if (ir->items[i + 2]->type == IRItem::INT)
        offset += ir->items[i + 2]->iVal * sizes[i];
      else
        temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
    }
    switch (ir->items[1]->symbol->symbolType) {
    case Symbol::CONST:
    case Symbol::GLOBAL_VAR:
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(ASMItem::A2),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(ASMItem::A2),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      break;
    case Symbol::LOCAL_VAR:
      moveFromSP(asms, ASMItem::A2, offsets[ir->items[1]->symbol]);
      break;
    case Symbol::PARAM:
      if (ir->items[1]->symbol->dimensions.empty())
        moveFromSP(asms, ASMItem::A2, offsets[ir->items[1]->symbol]);
      else
        loadFromSP(asms, ASMItem::A2, offsets[ir->items[1]->symbol]);
      break;
    default:
      break;
    }
    for (pair<unsigned, unsigned> temp : temps) {
      if (itemp2Reg.find(temp.first) == itemp2Reg.end()) {
        loadFromSP(asms, ASMItem::A3, spillOffsets[temp.first]);
        loadImmToReg(asms, ASMItem::A4, (int)temp.second);
        asms.push_back(new ASM(ASM::MUL, {new ASMItem(ASMItem::A3),
                                          new ASMItem(ASMItem::A3),
                                          new ASMItem(ASMItem::A4)}));
        asms.push_back(new ASM(ASM::ADD, {new ASMItem(ASMItem::A2),
                                          new ASMItem(ASMItem::A2),
                                          new ASMItem(ASMItem::A3)}));
      } else {
        loadImmToReg(asms, ASMItem::A3, (int)temp.second);
        asms.push_back(new ASM(ASM::MUL, {new ASMItem(ASMItem::A3),
                                          new ASMItem(itemp2Reg[temp.first]),
                                          new ASMItem(ASMItem::A3)}));
        asms.push_back(new ASM(ASM::ADD, {new ASMItem(ASMItem::A2),
                                          new ASMItem(ASMItem::A2),
                                          new ASMItem(ASMItem::A3)}));
      }
    }
    if (offset) {
      loadImmToReg(asms, ASMItem::A3, (int)offset);
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::A2),
                             new ASMItem(ASMItem::A3)}));
    }
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end()) {
      asms.push_back(new ASM(
          ASM::LDR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::A2)}));
      storeFromSP(asms, ASMItem::A2, spillOffsets[ir->items[0]->iVal]);
    } else
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                              new ASMItem(ASMItem::A2)}));
    break;
  }
  default:
    break;
  }
}

void ASMParser::parseMovToItemp(vector<ASM *> &asms, IR *ir) {
  switch (ir->items[1]->type) {
  case IRItem::INT:
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end()) {
      loadImmToReg(asms, ASMItem::A1, ir->items[1]->iVal);
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
    } else
      loadImmToReg(asms, itemp2Reg[ir->items[0]->iVal], ir->items[1]->iVal);
    break;
  case IRItem::ITEMP:
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end()) {
      if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end()) {
        loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[1]->iVal]);
        storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
      } else
        storeFromSP(asms, itemp2Reg[ir->items[1]->iVal],
                    spillOffsets[ir->items[0]->iVal]);
    } else {
      if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end())
        loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                   spillOffsets[ir->items[1]->iVal]);
      else
        asms.push_back(
            new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                               new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
    }
    break;
  case IRItem::RETURN:
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
    else
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(ASMItem::A1)}));
    break;
  case IRItem::SYMBOL: {
    vector<unsigned> sizes({4});
    for (int i = ir->items[1]->symbol->dimensions.size() - 1; i > 0; i--)
      sizes.push_back(sizes.back() * ir->items[1]->symbol->dimensions[i]);
    reverse(sizes.begin(), sizes.end());
    unsigned offset = 0;
    vector<pair<unsigned, unsigned>> temps;
    for (unsigned i = 0; i < ir->items.size() - 2; i++) {
      if (ir->items[i + 2]->type == IRItem::INT)
        offset += ir->items[i + 2]->iVal * sizes[i];
      else
        temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
    }
    switch (ir->items[1]->symbol->symbolType) {
    case Symbol::CONST:
    case Symbol::GLOBAL_VAR:
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(ASMItem::A2),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(ASMItem::A2),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      break;
    case Symbol::LOCAL_VAR:
      moveFromSP(asms, ASMItem::A2, offsets[ir->items[1]->symbol]);
      break;
    case Symbol::PARAM:
      if (ir->items[1]->symbol->dimensions.empty())
        moveFromSP(asms, ASMItem::A2, offsets[ir->items[1]->symbol]);
      else
        loadFromSP(asms, ASMItem::A2, offsets[ir->items[1]->symbol]);
      break;
    default:
      break;
    }
    for (pair<unsigned, unsigned> temp : temps) {
      if (itemp2Reg.find(temp.first) == itemp2Reg.end()) {
        loadFromSP(asms, ASMItem::A3, spillOffsets[temp.first]);
        loadImmToReg(asms, ASMItem::A4, (int)temp.second);
        asms.push_back(new ASM(ASM::MUL, {new ASMItem(ASMItem::A3),
                                          new ASMItem(ASMItem::A3),
                                          new ASMItem(ASMItem::A4)}));
        asms.push_back(new ASM(ASM::ADD, {new ASMItem(ASMItem::A2),
                                          new ASMItem(ASMItem::A2),
                                          new ASMItem(ASMItem::A3)}));
      } else {
        loadImmToReg(asms, ASMItem::A3, (int)temp.second);
        asms.push_back(new ASM(ASM::MUL, {new ASMItem(ASMItem::A3),
                                          new ASMItem(itemp2Reg[temp.first]),
                                          new ASMItem(ASMItem::A3)}));
        asms.push_back(new ASM(ASM::ADD, {new ASMItem(ASMItem::A2),
                                          new ASMItem(ASMItem::A2),
                                          new ASMItem(ASMItem::A3)}));
      }
    }
    if (offset) {
      loadImmToReg(asms, ASMItem::A3, (int)offset);
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::A2),
                             new ASMItem(ASMItem::A3)}));
    }
    if (ir->items[1]->symbol->dimensions.size() + 2 == ir->items.size())
      asms.push_back(new ASM(
          ASM::LDR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::A2)}));
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
      storeFromSP(asms, ASMItem::A2, spillOffsets[ir->items[0]->iVal]);
    else
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(ASMItem::A2)}));
    break;
  }
  default:
    break;
  }
}

void ASMParser::parseMovToSymbol(vector<ASM *> &asms, IR *ir) {
  vector<unsigned> sizes({4});
  for (int i = ir->items[0]->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * ir->items[0]->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(ASMItem::A2),
                    new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(ASMItem::A2),
                    new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, ASMItem::A2, offsets[ir->items[0]->symbol]);
    break;
  case Symbol::PARAM:
    if (ir->items[0]->symbol->dimensions.empty())
      moveFromSP(asms, ASMItem::A2, offsets[ir->items[0]->symbol]);
    else
      loadFromSP(asms, ASMItem::A2, offsets[ir->items[0]->symbol]);
    break;
  default:
    break;
  }
  for (pair<unsigned, unsigned> temp : temps) {
    if (itemp2Reg.find(temp.first) == itemp2Reg.end()) {
      loadFromSP(asms, ASMItem::A3, spillOffsets[temp.first]);
      loadImmToReg(asms, ASMItem::A4, (int)temp.second);
      asms.push_back(
          new ASM(ASM::MUL, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::A3),
                             new ASMItem(ASMItem::A4)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::A2),
                             new ASMItem(ASMItem::A3)}));
    } else {
      loadImmToReg(asms, ASMItem::A3, (int)temp.second);
      asms.push_back(new ASM(ASM::MUL, {new ASMItem(ASMItem::A3),
                                        new ASMItem(itemp2Reg[temp.first]),
                                        new ASMItem(ASMItem::A3)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::A2),
                             new ASMItem(ASMItem::A3)}));
    }
  }
  if (offset) {
    loadImmToReg(asms, ASMItem::A3, (int)offset);
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::A3),
                           new ASMItem(ASMItem::A3)}));
  }
  if (ir->items[0]->symbol->dataType == Symbol::FLOAT) {
    switch (ir->items[1]->type) {
    case IRItem::FLOAT:
      loadImmToReg(asms, ASMItem::A3, ir->items[1]->iVal);
      asms.push_back(new ASM(
          ASM::STR, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::A2)}));
      break;
    case IRItem::FTEMP:
      if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end()) {
        loadFromSP(asms, ASMItem::A3, spillOffsets[ir->items[1]->iVal]);
        asms.push_back(new ASM(
            ASM::STR, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::A2)}));
      } else
        asms.push_back(
            new ASM(ASM::VSTR, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                                new ASMItem(ASMItem::A2)}));
      break;
    case IRItem::RETURN:
      asms.push_back(new ASM(
          ASM::VSTR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::A2)}));
      break;
    default:
      break;
    }
  } else {
    switch (ir->items[1]->type) {
    case IRItem::INT:
      loadImmToReg(asms, ASMItem::A3, ir->items[1]->iVal);
      asms.push_back(new ASM(
          ASM::STR, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::A2)}));
      break;
    case IRItem::ITEMP:
      if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end()) {
        loadFromSP(asms, ASMItem::A3, spillOffsets[ir->items[1]->iVal]);
        asms.push_back(new ASM(
            ASM::STR, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::A2)}));
      } else
        asms.push_back(
            new ASM(ASM::STR, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                               new ASMItem(ASMItem::A2)}));
      break;
    case IRItem::RETURN:
      asms.push_back(new ASM(
          ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A2)}));
      break;
    default:
      break;
    }
  }
}

void ASMParser::parseNeg(vector<ASM *> &asms, IR *ir) {
  if (ir->items[0]->type == IRItem::ITEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    asms.push_back(new ASM(
        ASM::RSB,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(0)}));
    if (flag1)
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
  } else {
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S1, spillOffsets[ir->items[1]->iVal]);
    asms.push_back(new ASM(
        ASM::VNEG,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal])}));
    if (flag1)
      storeFromSP(asms, ASMItem::S0, spillOffsets[ir->items[0]->iVal]);
  }
}

void ASMParser::parseReturn(vector<ASM *> &asms, IR *ir, IR *lastIR) {
  if (!ir->items.empty()) {
    if (ir->items[0]->type == IRItem::ITEMP) {
      if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
        loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
      else
        asms.push_back(
            new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                               new ASMItem(itemp2Reg[ir->items[0]->iVal])}));
    } else {
      if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
        loadFromSP(asms, ASMItem::S0, spillOffsets[ir->items[0]->iVal]);
      else
        asms.push_back(
            new ASM(ASM::VMOV, {new ASMItem(ASMItem::S0),
                                new ASMItem(ftemp2Reg[ir->items[0]->iVal])}));
    }
  }
  asms.push_back(
      new ASM(ASM::B, {new ASMItem(ASMItem::LABEL, irLabels[lastIR])}));
}

void ASMParser::popArgs(vector<ASM *> &asms) {
  ASM *tempASM = new ASM(ASM::VPOP, {});
  for (unsigned i = 0; i < usedRegNum[1]; i++)
    tempASM->items.push_back(new ASMItem(vFRegs[i]));
  if (tempASM->items.empty())
    delete tempASM;
  else
    asms.push_back(tempASM);
  tempASM = new ASM(ASM::POP, {});
  for (unsigned i = 0; i < usedRegNum[0]; i++)
    tempASM->items.push_back(new ASMItem(vIRegs[i]));
  tempASM->items.push_back(new ASMItem(ASMItem::PC));
  asms.push_back(tempASM);
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
    for (IR *ir : funcIR.second)
      if (ir->type == IR::LABEL)
        irLabels[ir] = labelId++;
  }
}

void ASMParser::saveArgRegs(vector<ASM *> &asms, Symbol *func) {
  unsigned iCnt = 0, fCnt = 0, offset = 0;
  for (unsigned i = 0; i < func->params.size(); i++) {
    if (!func->params[i]->dimensions.empty() ||
        func->params[i]->dataType == Symbol::INT) {
      if (iCnt < 4) {
        asms.push_back(new ASM(ASM::PUSH, {new ASMItem(aIRegs[iCnt++])}));
        offsets[func->params[i]] = -4 * (min(iCnt, 4u) + min(fCnt, 16u));
        savedRegs++;
      } else {
        offsets[func->params[i]] =
            offset + (usedRegNum[0] + usedRegNum[1] + 1) * 4;
        offset += 4;
      }
    } else {
      if (fCnt < 16) {
        asms.push_back(new ASM(ASM::VPUSH, {new ASMItem(aFRegs[fCnt++])}));
        offsets[func->params[i]] = -4 * (min(iCnt, 4u) + min(fCnt, 16u));
        savedRegs++;
      } else {
        offsets[func->params[i]] =
            offset + (usedRegNum[0] + usedRegNum[1] + 1) * 4;
        offset += 4;
      }
    }
  }
}

void ASMParser::saveUsedRegs(vector<ASM *> &asms) {
  ASM *tempASM = new ASM(ASM::PUSH, {});
  for (unsigned i = 0; i < usedRegNum[0]; i++)
    tempASM->items.push_back(new ASMItem(vIRegs[i]));
  tempASM->items.push_back(new ASMItem(ASMItem::LR));
  asms.push_back(tempASM);
  tempASM = new ASM(ASM::VPUSH, {});
  for (unsigned i = 0; i < usedRegNum[1]; i++)
    tempASM->items.push_back(new ASMItem(vFRegs[i]));
  if (tempASM->items.empty())
    delete tempASM;
  else
    asms.push_back(tempASM);
}

void ASMParser::storeFromSP(vector<ASM *> &asms, ASMItem::RegType source,
                            unsigned offset) {
  vector<unsigned> smartBytes;
  for (int i = 30; i >= 0; i -= 2) {
    if (offset & (0x3 << i)) {
      if (i >= 6)
        smartBytes.push_back(offset & (0xff << (i - 6)));
      else
        smartBytes.push_back(offset & (0xff >> (6 - i)));
      i -= 6;
    }
  }
  ASM::ASMOpType op = isFloatReg(source) ? ASM::VSTR : ASM::STR;
  unsigned maxOffset = isFloatReg(source) ? 1020 : 4095;
  switch (smartBytes.size()) {
  case 0:
    asms.push_back(
        new ASM(op, {new ASMItem(source), new ASMItem(ASMItem::SP)}));
    break;
  case 1:
    if (offset <= maxOffset)
      asms.push_back(new ASM(op, {new ASMItem(source), new ASMItem(ASMItem::SP),
                                  new ASMItem(offset)}));
    else {
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::SP),
                             new ASMItem(offset)}));
      asms.push_back(
          new ASM(op, {new ASMItem(source), new ASMItem(ASMItem::A4)}));
    }
    break;
  case 2:
    if (offset <= maxOffset) {
      asms.push_back(new ASM(op, {new ASMItem(source), new ASMItem(ASMItem::SP),
                                  new ASMItem(offset)}));
    } else if (!(offset & 0xffff0000)) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(offset)}));
      asms.push_back(new ASM(op, {new ASMItem(source), new ASMItem(ASMItem::SP),
                                  new ASMItem(ASMItem::A4)}));
    } else if (smartBytes[1] <= maxOffset) {
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::SP),
                             new ASMItem(smartBytes[0])}));
      asms.push_back(new ASM(op, {new ASMItem(source), new ASMItem(ASMItem::A4),
                                  new ASMItem(smartBytes[1])}));
    } else {
      asms.push_back(new ASM(
          ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(offset & 0xffff)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(ASMItem::A4), new ASMItem(offset >> 16)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::SP),
                             new ASMItem(ASMItem::A4)}));
      asms.push_back(new ASM(op, {new ASMItem(source), new ASMItem(ASMItem::SP),
                                  new ASMItem(ASMItem::A4)}));
    }
    break;
  default:
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(offset & 0xffff)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(ASMItem::A4), new ASMItem(offset >> 16)}));
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::SP),
                           new ASMItem(ASMItem::A4)}));
    asms.push_back(new ASM(op, {new ASMItem(source), new ASMItem(ASMItem::SP),
                                new ASMItem(ASMItem::A4)}));
    break;
  }
}
