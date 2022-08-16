#include <algorithm>
#include <iostream>

#include "ASMParser.h"
#include "HardCoding.h"
#include "LinearRegAllocator.h"

using namespace std;

ASMParser::ASMParser(pair<unsigned, unsigned> lineno,
                     unordered_map<Symbol *, vector<IR *>> &funcIRs,
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
  for (unordered_map<Symbol *, vector<ASM *>>::iterator it = funcASMs.begin();
       it != funcASMs.end(); it++)
    for (ASM *a : it->second)
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

bool ASMParser::canBeLoadInSingleInstruction(unsigned imm) {
  if (imm <= 0xffff)
    return true;
  if (isByteShiftImm(imm))
    return true;
  if (isByteShiftImm(~imm))
    return true;
  return false;
}

unordered_map<Symbol *, vector<ASM *>> ASMParser::getFuncASMs() {
  if (!isProcessed)
    parse();
  return funcASMs;
}

unsigned ASMParser::float2Unsigned(float fVal) {
  union {
    float fVal;
    unsigned uVal;
  } unionData;
  unionData.fVal = fVal;
  return unionData.uVal;
}

void ASMParser::loadFromReg(vector<ASM *> &asms, Reg::Type target,
                            Reg::Type base, unsigned offset) {
  if (isFloatReg(target)) {
    if (!offset) {
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(target), new ASMItem(base)}));
      return;
    }
    if (offset <= 1020) {
      asms.push_back(new ASM(ASM::VLDR, {new ASMItem(target), new ASMItem(base),
                                         new ASMItem(offset)}));
      return;
    }
    moveFromReg(asms, Reg::A4, base, offset);
    asms.push_back(
        new ASM(ASM::VLDR, {new ASMItem(target), new ASMItem(Reg::A4)}));
  } else {
    if (!offset) {
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(target), new ASMItem(base)}));
      return;
    }
    if (offset <= 4095) {
      asms.push_back(new ASM(ASM::LDR, {new ASMItem(target), new ASMItem(base),
                                        new ASMItem(offset)}));
      return;
    }
    loadImmToReg(asms, Reg::A4, offset);
    asms.push_back(new ASM(ASM::LDR, {new ASMItem(target), new ASMItem(base),
                                      new ASMItem(Reg::A4)}));
  }
}

void ASMParser::loadFromSP(vector<ASM *> &asms, Reg::Type target,
                           unsigned offset) {
  if (isFloatReg(target)) {
    if (!offset) {
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(target), new ASMItem(Reg::SP)}));
      return;
    }
    if (offset <= 1020) {
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(target), new ASMItem(Reg::SP),
                              new ASMItem(offset)}));
      return;
    }
    loadImmToReg(asms, Reg::A4, offset);
    asms.push_back(
        new ASM(ASM::VLDR, {new ASMItem(target), new ASMItem(Reg::SP),
                            new ASMItem(Reg::A4)}));
  } else {
    if (!offset) {
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(target), new ASMItem(Reg::SP)}));
      return;
    }
    if (offset <= 4095) {
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(target), new ASMItem(Reg::SP),
                             new ASMItem(offset)}));
      return;
    }
    moveFromSP(asms, Reg::A4, offset);
    asms.push_back(
        new ASM(ASM::LDR, {new ASMItem(target), new ASMItem(Reg::A4)}));
  }
}

void ASMParser::loadImmToReg(vector<ASM *> &asms, Reg::Type reg, float val) {
  unsigned data = float2Unsigned(val);
  int exp = (data >> 23) & 0xff;
  if (exp >= 124 && exp <= 131 && !(data & 0x7ffff)) {
    asms.push_back(new ASM(ASM::VMOV, {new ASMItem(reg), new ASMItem(data)}));
    return;
  }
  loadImmToReg(asms, Reg::A4, data);
  asms.push_back(new ASM(ASM::VMOV, {new ASMItem(reg), new ASMItem(Reg::A4)}));
}

void ASMParser::loadImmToReg(vector<ASM *> &asms, Reg::Type reg, unsigned val) {
  if (val <= 0xffff || isByteShiftImm(val)) {
    asms.push_back(new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(val)}));
    return;
  }
  if (isByteShiftImm(~val)) {
    asms.push_back(new ASM(ASM::MVN, {new ASMItem(reg), new ASMItem(~val)}));
    return;
  }
  asms.push_back(
      new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(val & 0xffff)}));
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

bool ASMParser::isByteShiftImm(unsigned imm) {
  unsigned mask = 0xffffff;
  for (int i = 0; i < 16; i++) {
    if (!(imm & mask))
      return true;
    mask = (mask << 2) | (mask >> 30);
  }
  return false;
}

bool ASMParser::isFloatImm(float imm) {
  unsigned data = float2Unsigned(imm);
  int exp = (data >> 23) & 0xff;
  return exp >= 124 && exp <= 131 && !(data & 0x7ffff);
}

bool ASMParser::isFloatReg(Reg::Type reg) { return reg >= Reg::S0; }

void ASMParser::makeFrame(vector<ASM *> &asms, vector<IR *> &irs,
                          Symbol *func) {
  LinearRegAllocator *allocator = new LinearRegAllocator(irs);
  usedRegNum = allocator->getUsedRegNum();
  itemp2Reg = allocator->getItemp2Reg();
  ftemp2Reg = allocator->getFtemp2Reg();
  temp2SpillReg = allocator->getTemp2SpillReg();
  irs = allocator->getIRs();
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
  moveFromSP(asms, Reg::SP, (int)savedRegs * 4 - frameOffset);
}

void ASMParser::moveFromReg(vector<ASM *> &asms, Reg::Type target,
                            Reg::Type base, int offset) {
  if (target == base && !offset)
    return;
  if (isByteShiftImm(offset))
    asms.push_back(new ASM(ASM::ADD, {new ASMItem(target), new ASMItem(base),
                                      new ASMItem(offset)}));
  else if (isByteShiftImm(-offset))
    asms.push_back(new ASM(ASM::SUB, {new ASMItem(target), new ASMItem(base),
                                      new ASMItem(-offset)}));
  else if (canBeLoadInSingleInstruction(offset) ||
           canBeLoadInSingleInstruction(-offset)) {
    loadImmToReg(asms, Reg::A4, (unsigned)offset);
    asms.push_back(new ASM(ASM::ADD, {new ASMItem(target), new ASMItem(base),
                                      new ASMItem(Reg::A4)}));
  } else {
    loadImmToReg(asms, Reg::A4, (unsigned)(-offset));
    asms.push_back(new ASM(ASM::SUB, {new ASMItem(target), new ASMItem(base),
                                      new ASMItem(Reg::A4)}));
  }
}

void ASMParser::moveFromSP(vector<ASM *> &asms, Reg::Type target, int offset) {
  if (target == Reg::SP && !offset)
    return;
  if (isByteShiftImm(offset))
    asms.push_back(new ASM(ASM::ADD, {new ASMItem(target), new ASMItem(Reg::SP),
                                      new ASMItem(offset)}));
  else if (isByteShiftImm(-offset))
    asms.push_back(new ASM(ASM::SUB, {new ASMItem(target), new ASMItem(Reg::SP),
                                      new ASMItem(-offset)}));
  else if (canBeLoadInSingleInstruction(offset) ||
           canBeLoadInSingleInstruction(-offset)) {
    loadImmToReg(asms, Reg::A4, (unsigned)offset);
    asms.push_back(new ASM(ASM::ADD, {new ASMItem(target), new ASMItem(Reg::SP),
                                      new ASMItem(Reg::A4)}));
  } else {
    loadImmToReg(asms, Reg::A4, (unsigned)(-offset));
    asms.push_back(new ASM(ASM::SUB, {new ASMItem(target), new ASMItem(Reg::SP),
                                      new ASMItem(Reg::A4)}));
  }
}

void ASMParser::mulRegValue(vector<ASM *> &asms, Reg::Type target,
                            Reg::Type source, int val) {
  if (!val)
    loadImmToReg(asms, target, 0u);
  else if (val == 1)
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(target), new ASMItem(source)}));
  else if (val == -1)
    asms.push_back(new ASM(
        ASM::RSB, {new ASMItem(target), new ASMItem(source), new ASMItem(0)}));
  else if (num2powerMap.find(val) != num2powerMap.end())
    asms.push_back(new ASM(ASM::LSL, {new ASMItem(target), new ASMItem(source),
                                      new ASMItem(num2powerMap[val])}));
  else if (num2power2Map.find(val) != num2power2Map.end() && (val & 0x1) == 0x1)
    asms.push_back(
        new ASM(ASM::ADD,
                {new ASMItem(target), new ASMItem(source), new ASMItem(source),
                 new ASMItem(ASMItem::LSL, num2power2Map[val].second)}));
  else if (num2lineMap.find(val) != num2lineMap.end() &&
           !num2lineMap[val].first)
    asms.push_back(
        new ASM(ASM::RSB,
                {new ASMItem(target), new ASMItem(source), new ASMItem(source),
                 new ASMItem(ASMItem::LSL, num2lineMap[val].second + 1)}));
  else if (num2power2Map.find(val) != num2power2Map.end()) {
    asms.push_back(
        new ASM(ASM::ADD,
                {new ASMItem(target), new ASMItem(source), new ASMItem(source),
                 new ASMItem(ASMItem::LSL, num2power2Map[val].second -
                                               num2power2Map[val].first)}));
    asms.push_back(new ASM(ASM::LSL, {new ASMItem(target), new ASMItem(target),
                                      new ASMItem(num2power2Map[val].first)}));
  } else if (num2lineMap.find(val) != num2lineMap.end()) {
    asms.push_back(
        new ASM(ASM::RSB,
                {new ASMItem(target), new ASMItem(source), new ASMItem(source),
                 new ASMItem(ASMItem::LSL, num2lineMap[val].second -
                                               num2lineMap[val].first + 1)}));
    asms.push_back(new ASM(ASM::LSL, {new ASMItem(target), new ASMItem(target),
                                      new ASMItem(num2lineMap[val].first)}));
  } else {
    loadImmToReg(asms, target, (unsigned)val);
    asms.push_back(new ASM(ASM::MUL, {new ASMItem(target), new ASMItem(source),
                                      new ASMItem(target)}));
  }
}

void ASMParser::parse() {
  isProcessed = true;
  preProcess();
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++)
    funcASMs[it->first] = parseFunc(it->first, it->second);
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
        loadFromSP(asms, Reg::A1, spillOffsets[ir->items[i]->iVal]);
        storeFromSP(asms, Reg::A1, offset);
      } else
        storeFromSP(asms, itemp2Reg[ir->items[i]->iVal], offset);
    } else if (ir->items[i]->type == IRItem::FTEMP) {
      if (fCnt < 16) {
        fCnt++;
        continue;
      }
      if (ftemp2Reg.find(ir->items[i]->iVal) == ftemp2Reg.end()) {
        loadFromSP(asms, Reg::A1, spillOffsets[ir->items[i]->iVal]);
        storeFromSP(asms, Reg::A1, offset);
      } else
        storeFromSP(asms, ftemp2Reg[ir->items[i]->iVal], offset);
    } else if (ir->items[i]->type == IRItem::INT) {
      if (iCnt < 4) {
        iCnt++;
        continue;
      }
      loadImmToReg(asms, Reg::A1, (unsigned)ir->items[i]->iVal);
      storeFromSP(asms, Reg::A1, offset);
    } else if (ir->items[i]->type == IRItem::FLOAT) {
      if (fCnt < 16) {
        fCnt++;
        continue;
      }
      loadImmToReg(asms, Reg::A1, (unsigned)ir->items[i]->iVal);
      storeFromSP(asms, Reg::A1, offset);
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
    } else if (ir->items[i]->type == IRItem::FTEMP) {
      if (fCnt < 16) {
        if (ftemp2Reg.find(ir->items[i]->iVal) == ftemp2Reg.end())
          loadFromSP(asms, aFRegs[fCnt++], spillOffsets[ir->items[i]->iVal]);
        else
          asms.push_back(
              new ASM(ASM::VMOV, {new ASMItem(aFRegs[fCnt++]),
                                  new ASMItem(ftemp2Reg[ir->items[i]->iVal])}));
      }
    } else if (ir->items[i]->type == IRItem::INT) {
      if (iCnt < 4)
        loadImmToReg(asms, aIRegs[iCnt++], (unsigned)ir->items[i]->iVal);
    } else if (ir->items[i]->type == IRItem::FLOAT) {
      if (fCnt < 16)
        loadImmToReg(asms, aFRegs[fCnt++], ir->items[i]->fVal);
    }
  }
  if (!ir->items[0]->symbol->name.compare("_sysy_starttime"))
    loadImmToReg(asms, Reg::A1, startLineno);
  if (!ir->items[0]->symbol->name.compare("_sysy_stoptime"))
    loadImmToReg(asms, Reg::A1, stopLineno);
  asms.push_back(new ASM(ASM::BL, {new ASMItem(ir->items[0]->symbol->name)}));
}

vector<ASM *> ASMParser::parseFunc(Symbol *func, vector<IR *> &irs) {
  vector<ASM *> asms;
  initFrame();
  makeFrame(asms, irs, func);
  for (unsigned i = 0; i < irs.size(); i++) {
    switch (irs[i]->type) {
    case IR::ADD:
      parseAdd(asms, irs[i]);
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
    case IR::SUB:
      parseSub(asms, irs[i]);
      break;
    default:
      break;
    }
  }
  moveFromSP(asms, Reg::SP, frameOffset);
  popArgs(asms);
  return asms;
}

void ASMParser::parseI2F(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal],
               spillOffsets[ir->items[1]->iVal]);
  else
    asms.push_back(
        new ASM(ASM::VMOV,
                {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
                 new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
  asms.push_back(
      new ASM(ASM::VCVTSF,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseLNot(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end();
  switch (ir->items[1]->type) {
  case IRItem::ITEMP: {
    bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    Reg::Type targetReg = flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal];
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(new ASM(ASM::CMP, {new ASMItem(Reg::A1), new ASMItem(0)}));
    } else
      asms.push_back(
          new ASM(ASM::CMP, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                             new ASMItem(0)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::NE, {new ASMItem(targetReg), new ASMItem(0)}));
    if (flag1)
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    break;
  }
  case IRItem::FTEMP: {
    bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    Reg::Type targetReg = flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal];
    if (flag2) {
      loadFromSP(asms, Reg::S0, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(
          new ASM(ASM::VCMP, {new ASMItem(Reg::S0), new ASMItem(0.0f)}));
    } else {
      asms.push_back(
          new ASM(ASM::VCMP, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                              new ASMItem(0.0f)}));
    }
    asms.push_back(new ASM(ASM::VMRS, {}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::NE, {new ASMItem(targetReg), new ASMItem(0)}));
    if (flag1)
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    break;
  }
  default:
    break;
  }
}

void ASMParser::parseMemsetZero(vector<ASM *> &asms, IR *ir) {
  moveFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
  asms.push_back(new ASM(ASM::MOV, {new ASMItem(Reg::A2), new ASMItem(0)}));
  unsigned size = 4;
  for (int dimension : ir->items[0]->symbol->dimensions)
    size *= dimension;
  loadImmToReg(asms, Reg::A3, size);
  asms.push_back(new ASM(ASM::BL, {new ASMItem("memset")}));
}

void ASMParser::parseNeg(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
    asms.push_back(new ASM(
        ASM::RSB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(0)}));
    if (flag1)
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[1]->type == IRItem::FTEMP) {
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
    asms.push_back(new ASM(
        ASM::VNEG,
        {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal])}));
    if (flag1)
      storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
  }
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
  tempASM->items.push_back(new ASMItem(Reg::PC));
  asms.push_back(tempASM);
}

void ASMParser::preProcess() {
  if (!getenv("DRM")) {
    int id = 0;
    for (Symbol *symbol : consts)
      symbol->name = "var" + to_string(id++);
    for (Symbol *symbol : globalVars)
      symbol->name = "var" + to_string(id++);
    id = 0;
    for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
         it != funcIRs.end(); it++)
      if (it->first->name.compare("main"))
        it->first->name = "f" + to_string(id++);
  }
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++)
    for (IR *ir : it->second)
      if (ir->type == IR::LABEL)
        irLabels[ir] = labelId++;
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
  tempASM->items.push_back(new ASMItem(Reg::LR));
  asms.push_back(tempASM);
  tempASM = new ASM(ASM::VPUSH, {});
  for (unsigned i = 0; i < usedRegNum[1]; i++)
    tempASM->items.push_back(new ASMItem(vFRegs[i]));
  if (tempASM->items.empty())
    delete tempASM;
  else
    asms.push_back(tempASM);
}

void ASMParser::storeFromReg(vector<ASM *> &asms, Reg::Type source,
                             Reg::Type base, unsigned offset) {
  ASM::ASMOpType op = isFloatReg(source) ? ASM::VSTR : ASM::STR;
  unsigned maxOffset = isFloatReg(source) ? 1020 : 4095;
  if (!offset) {
    asms.push_back(new ASM(op, {new ASMItem(source), new ASMItem(base)}));
    return;
  }
  if (offset <= maxOffset) {
    asms.push_back(new ASM(
        op, {new ASMItem(source), new ASMItem(base), new ASMItem(offset)}));
    return;
  }
  loadImmToReg(asms, Reg::A4, offset);
  asms.push_back(new ASM(
      op, {new ASMItem(source), new ASMItem(base), new ASMItem(Reg::A4)}));
}

void ASMParser::storeFromSP(vector<ASM *> &asms, Reg::Type source,
                            unsigned offset) {
  ASM::ASMOpType op = isFloatReg(source) ? ASM::VSTR : ASM::STR;
  unsigned maxOffset = isFloatReg(source) ? 1020 : 4095;
  if (!offset) {
    asms.push_back(new ASM(op, {new ASMItem(source), new ASMItem(Reg::SP)}));
    return;
  }
  if (offset <= maxOffset) {
    asms.push_back(new ASM(
        op, {new ASMItem(source), new ASMItem(Reg::SP), new ASMItem(offset)}));
    return;
  }
  loadImmToReg(asms, Reg::A4, offset);
  asms.push_back(new ASM(
      op, {new ASMItem(source), new ASMItem(Reg::SP), new ASMItem(Reg::A4)}));
}
