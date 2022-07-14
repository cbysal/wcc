#include <iostream>

#include "ASMParser.h"

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

void ASMParser::allocReg(const vector<IR *> &irs) {
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
        freeVRegs.push_back(reg.second);
        removeList.push_back(reg.first);
      }
    for (unsigned id : removeList)
      tempVReg.erase(id);
    removeList.clear();
    for (const pair<unsigned, ASMItem::RegType> reg : tempSReg)
      if (i > lifespan[reg.first]) {
        freeSRegs.push_back(reg.second);
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
        if (!freeVRegs.empty()) {
          tempVReg[item->iVal] = freeVRegs.back();
          itemp2Reg[item->iVal] = freeVRegs.back();
          freeVRegs.pop_back();
          usedIRegs = max<int>(usedIRegs, 8 - freeVRegs.size());
        } else if (usedSpillRegs.size() == spillRegs) {
          tempSpillReg[item->iVal] = spillRegs;
          temp2SpillReg[item->iVal] = spillRegs;
          usedSpillRegs.insert(spillRegs);
          spillRegs++;
        } else {
          for (unsigned j = 0; j < spillRegs; j++)
            if (usedSpillRegs.find(j) == usedSpillRegs.end()) {
              tempSpillReg[item->iVal] = j;
              temp2SpillReg[item->iVal] = j;
              usedSpillRegs.insert(j);
              break;
            }
        }
      }
      if (item->type == IRItem::FTEMP &&
          tempSReg.find(item->iVal) == tempSReg.end() &&
          tempSpillReg.find(item->iVal) == tempSpillReg.end()) {
        if (!freeSRegs.empty()) {
          tempSReg[item->iVal] = freeSRegs.back();
          ftemp2Reg[item->iVal] = freeSRegs.back();
          freeSRegs.pop_back();
          usedFRegs = max<int>(usedFRegs, 16 - freeSRegs.size());
        } else if (usedSpillRegs.size() == spillRegs) {
          tempSpillReg[item->iVal] = spillRegs;
          temp2SpillReg[item->iVal] = spillRegs;
          usedSpillRegs.insert(spillRegs);
          spillRegs++;
        } else {
          for (unsigned j = 0; j < spillRegs; j++)
            if (usedSpillRegs.find(j) == usedSpillRegs.end()) {
              tempSpillReg[item->iVal] = j;
              temp2SpillReg[item->iVal] = j;
              usedSpillRegs.insert(j);
              break;
            }
        }
      }
    }
  }
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

void ASMParser::loadImmToReg(vector<ASM *> &asms, ASMItem::RegType reg,
                             float val) {
  unsigned data = *(unsigned *)(&val);
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

void ASMParser::loadOrStoreFromSP(vector<ASM *> &asms, bool isLoad,
                                  ASMItem::RegType targetOrSource,
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
  ASM::ASMOpType fOp = isLoad ? ASM::VLDR : ASM::VSTR;
  ASM::ASMOpType iOp = isLoad ? ASM::LDR : ASM::STR;
  ASM::ASMOpType op = isFloatReg(targetOrSource) ? fOp : iOp;
  unsigned maxOffset = isFloatReg(targetOrSource) ? 1020 : 4095;
  switch (smartBytes.size()) {
  case 0:
    asms.push_back(
        new ASM(op, {new ASMItem(targetOrSource), new ASMItem(ASMItem::SP)}));
    break;
  case 1:
    if (offset <= maxOffset)
      asms.push_back(
          new ASM(op, {new ASMItem(targetOrSource), new ASMItem(ASMItem::SP),
                       new ASMItem(offset)}));
    else {
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::SP),
                             new ASMItem(offset)}));
      asms.push_back(
          new ASM(op, {new ASMItem(targetOrSource), new ASMItem(ASMItem::A4)}));
    }
    break;
  case 2:
    if (offset <= maxOffset) {
      asms.push_back(
          new ASM(op, {new ASMItem(targetOrSource), new ASMItem(ASMItem::SP),
                       new ASMItem(offset)}));
    } else if (!(offset & 0xffff0000)) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(offset)}));
      asms.push_back(
          new ASM(op, {new ASMItem(targetOrSource), new ASMItem(ASMItem::SP),
                       new ASMItem(ASMItem::A4)}));
    } else if (smartBytes[1] <= maxOffset) {
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::SP),
                             new ASMItem(smartBytes[0])}));
      asms.push_back(
          new ASM(op, {new ASMItem(targetOrSource), new ASMItem(ASMItem::A4),
                       new ASMItem(smartBytes[1])}));
    } else {
      asms.push_back(new ASM(
          ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(offset & 0xffff)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(ASMItem::A4), new ASMItem(offset >> 16)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::SP),
                             new ASMItem(ASMItem::A4)}));
      asms.push_back(
          new ASM(op, {new ASMItem(targetOrSource), new ASMItem(ASMItem::SP),
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
    asms.push_back(
        new ASM(op, {new ASMItem(targetOrSource), new ASMItem(ASMItem::SP),
                     new ASMItem(ASMItem::A4)}));
    break;
  }
}

void ASMParser::initFrame() {
  usedIRegs = 0;
  usedFRegs = 0;
  spillRegs = 0;
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
  allocReg(irs);
  int callArgSize = calcCallArgSize(irs);
  for (unordered_map<unsigned, int>::iterator it = temp2SpillReg.begin();
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
  frameOffset = callArgSize + spillRegs * 4 + localVarSize + savedRegs * 4;
  if ((frameOffset + (usedIRegs + usedFRegs) * 4) % 8 == 0)
    frameOffset += 4;
  for (unordered_map<Symbol *, int>::iterator it = offsets.begin();
       it != offsets.end(); it++)
    it->second += frameOffset;
  moveFromSP(asms, ASMItem::SP, (int)savedRegs * 4 - frameOffset);
}

void ASMParser::moveFromSP(vector<ASM *> &asms, ASMItem::RegType target,
                           int offset) {
  vector<unsigned> smartBytes(5);
  for (int i = 0; i < 4; i++) {
    vector<unsigned> temps;
    for (unsigned j = 0; j < 16; j++) {
      if (abs(offset) & (0x3 << (((i + j) % 16) * 2))) {
        temps.push_back(abs(offset) &
                        ((0xff << min<unsigned>(((i + j) % 16) * 2, 32)) |
                         max(0xff >> (((i + j) % 16) * 2 - 24), 0)));
        j += 3;
      }
    }
    if (smartBytes.size() > temps.size())
      smartBytes = temps;
  }
  ASM::ASMOpType op = offset > 0 ? ASM::ADD : ASM::SUB;
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
    removeUnusedLabels(asms);
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
      loadOrStoreFromSP(asms, true, ASMItem::A2,
                        spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadOrStoreFromSP(asms, true, ASMItem::A3,
                        spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        iOp,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::A3 : itemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      loadOrStoreFromSP(asms, false, ASMItem::A1,
                        spillOffsets[ir->items[0]->iVal]);
  } else {
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
         flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadOrStoreFromSP(asms, true, ASMItem::S1,
                        spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadOrStoreFromSP(asms, true, ASMItem::S2,
                        spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        fOp,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::S2 : ftemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      loadOrStoreFromSP(asms, false, ASMItem::S0,
                        spillOffsets[ir->items[2]->iVal]);
  }
}

void ASMParser::parseB(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP) {
    asms.push_back(
        new ASM(ASM::CMP, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                           ir->items[2]->type == IRItem::INT
                               ? new ASMItem(ir->items[2]->iVal)
                               : new ASMItem(itemp2Reg[ir->items[2]->iVal])}));
  } else {
    if (ir->items[2]->type == IRItem::FLOAT) {
      loadImmToReg(asms, ASMItem::S0, ir->items[1]->fVal);
      asms.push_back(
          new ASM(ASM::VCMP, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                              new ASMItem(ASMItem::S0)}));
    } else {
      asms.push_back(
          new ASM(ASM::VCMP, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                              new ASMItem(ftemp2Reg[ir->items[2]->iVal])}));
    }
    asms.push_back(new ASM(ASM::VMRS, {}));
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
        loadOrStoreFromSP(asms, true, ASMItem::A1,
                          spillOffsets[ir->items[i]->iVal]);
        loadOrStoreFromSP(asms, false, ASMItem::A1, offset);
      } else
        loadOrStoreFromSP(asms, false, itemp2Reg[ir->items[i]->iVal], offset);
    } else {
      if (fCnt < 16) {
        fCnt++;
        continue;
      }
      if (ftemp2Reg.find(ir->items[i]->iVal) == ftemp2Reg.end()) {
        loadOrStoreFromSP(asms, true, ASMItem::A1,
                          spillOffsets[ir->items[i]->iVal]);
        loadOrStoreFromSP(asms, false, ASMItem::A1, offset);
      } else
        loadOrStoreFromSP(asms, false, ftemp2Reg[ir->items[i]->iVal], offset);
    }
    offset += 4;
  }
  iCnt = 0;
  fCnt = 0;
  for (unsigned i = 1; i < ir->items.size(); i++) {
    if (ir->items[i]->type == IRItem::ITEMP) {
      if (iCnt < 4) {
        if (itemp2Reg.find(ir->items[i]->iVal) == itemp2Reg.end())
          loadOrStoreFromSP(asms, true, aIRegs[iCnt++],
                            spillOffsets[ir->items[i]->iVal]);
        else
          asms.push_back(
              new ASM(ASM::MOV, {new ASMItem(aIRegs[iCnt++]),
                                 new ASMItem(itemp2Reg[ir->items[i]->iVal])}));
      }
    } else {
      if (fCnt < 16) {
        if (ftemp2Reg.find(ir->items[i]->iVal) == ftemp2Reg.end())
          loadOrStoreFromSP(asms, true, aFRegs[fCnt++],
                            spillOffsets[ir->items[i]->iVal]);
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
    bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
         flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
    if (flag2)
      loadOrStoreFromSP(asms, true, ASMItem::A1,
                        spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadOrStoreFromSP(asms, true, ASMItem::A2,
                        spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        ASM::CMP,
        {new ASMItem(flag2 ? ASMItem::A1 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::A2 : itemp2Reg[ir->items[2]->iVal])}));
  } else {
    bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
         flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadOrStoreFromSP(asms, true, ASMItem::S0,
                        spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadOrStoreFromSP(asms, true, ASMItem::S1,
                        spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        ASM::VCMP,
        {new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::S1 : ftemp2Reg[ir->items[2]->iVal])}));
    asms.push_back(new ASM(ASM::VMRS, {}));
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
    loadOrStoreFromSP(asms, false, ASMItem::A1,
                      spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseF2I(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadOrStoreFromSP(asms, true, ASMItem::S0,
                      spillOffsets[ir->items[0]->iVal]);
  asms.push_back(new ASM(
      ASM::VCVTFS,
      {new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal]),
       new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal])}));
  asms.push_back(new ASM(
      ASM::VMOV,
      {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
       new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal])}));
  if (flag2)
    loadOrStoreFromSP(asms, false, ASMItem::A1,
                      spillOffsets[ir->items[0]->iVal]);
}

vector<ASM *> ASMParser::parseFunc(Symbol *func, const vector<IR *> &irs) {
  vector<ASM *> asms;
  initFrame();
  makeFrame(asms, irs, func);
  for (unsigned i = 0; i < irs.size(); i++) {
    asms.push_back(
        new ASM(ASM::LABEL, {new ASMItem(ASMItem::LABEL, irLabels[irs[i]])}));
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
    case IR::FUNC_END:
      moveFromSP(asms, ASMItem::SP, frameOffset);
      popArgs(asms);
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
    case IR::LOAD:
      parseLoad(asms, irs[i]);
      break;
    case IR::MEMSET_ZERO:
      parseMemsetZero(asms, irs[i]);
      break;
    case IR::MOV:
      parseMov(asms, irs[i]);
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
    case IR::STORE:
      parseStore(asms, irs[i]);
      break;
    case IR::SUB:
      parseAlgo(asms, ASM::SUB, ASM::VSUB, irs[i]);
      break;
    default:
      break;
    }
  }
  return asms;
}

void ASMParser::parseI2F(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag2)
    loadOrStoreFromSP(asms, true, ASMItem::S0,
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
  if (flag2)
    loadOrStoreFromSP(asms, false, ASMItem::S0,
                      spillOffsets[ir->items[1]->iVal]);
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
      loadOrStoreFromSP(asms, true, itemp2Reg[ir->items[0]->iVal],
                        spillOffsets[ir->items[1]->iVal]);
    asms.push_back(new ASM(ASM::CMP, {new ASMItem(targetReg), new ASMItem(0)}));
  }
  asms.push_back(
      new ASM(ASM::MOV, ASM::EQ, {new ASMItem(targetReg), new ASMItem(1)}));
  asms.push_back(
      new ASM(ASM::MOV, ASM::NE, {new ASMItem(targetReg), new ASMItem(0)}));
  if (flag)
    loadOrStoreFromSP(asms, false, targetReg, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseLoad(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag1)
    loadOrStoreFromSP(asms, true, ASMItem::A1,
                      spillOffsets[ir->items[1]->iVal]);
  if (ir->items[0]->type == IRItem::ITEMP) {
    bool flag2 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end();
    asms.push_back(new ASM(
        ASM::LDR,
        {new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[1]->iVal])}));
    if (flag2)
      loadOrStoreFromSP(asms, false, ASMItem::A2,
                        spillOffsets[ir->items[0]->iVal]);
  } else {
    bool flag2 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end();
    asms.push_back(new ASM(
        ASM::VLDR,
        {new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[1]->iVal])}));
    if (flag2)
      loadOrStoreFromSP(asms, false, ASMItem::S0,
                        spillOffsets[ir->items[0]->iVal]);
  }
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

void ASMParser::parseMov(vector<ASM *> &asms, IR *ir) {
  switch (ir->items[1]->type) {
  case IRItem::FLOAT:
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end()) {
      loadImmToReg(asms, ASMItem::A1, ir->items[1]->iVal);
      loadOrStoreFromSP(asms, false, ASMItem::A1,
                        spillOffsets[ir->items[0]->iVal]);
    } else
      loadImmToReg(asms, ftemp2Reg[ir->items[0]->iVal], ir->items[1]->fVal);
    break;
  case IRItem::FTEMP:
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end()) {
      if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end()) {
        loadOrStoreFromSP(asms, true, ASMItem::S0,
                          spillOffsets[ir->items[1]->iVal]);
        loadOrStoreFromSP(asms, false, ASMItem::S0,
                          spillOffsets[ir->items[0]->iVal]);
      } else
        loadOrStoreFromSP(asms, false, ftemp2Reg[ir->items[1]->iVal],
                          spillOffsets[ir->items[0]->iVal]);
    } else {
      if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end())
        loadOrStoreFromSP(asms, true, ftemp2Reg[ir->items[0]->iVal],
                          spillOffsets[ir->items[1]->iVal]);
      else
        asms.push_back(
            new ASM(ASM::VMOV, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                                new ASMItem(ftemp2Reg[ir->items[1]->iVal])}));
    }
    break;
  case IRItem::INT:
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end()) {
      loadImmToReg(asms, ASMItem::A1, ir->items[1]->iVal);
      loadOrStoreFromSP(asms, false, ASMItem::A1,
                        spillOffsets[ir->items[0]->iVal]);
    } else
      loadImmToReg(asms, itemp2Reg[ir->items[0]->iVal], ir->items[1]->iVal);
    break;
  case IRItem::ITEMP:
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end()) {
      if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end()) {
        loadOrStoreFromSP(asms, true, ASMItem::A1,
                          spillOffsets[ir->items[1]->iVal]);
        loadOrStoreFromSP(asms, false, ASMItem::A1,
                          spillOffsets[ir->items[0]->iVal]);
      } else
        loadOrStoreFromSP(asms, false, itemp2Reg[ir->items[1]->iVal],
                          spillOffsets[ir->items[0]->iVal]);
    } else {
      if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end())
        loadOrStoreFromSP(asms, true, itemp2Reg[ir->items[0]->iVal],
                          spillOffsets[ir->items[1]->iVal]);
      else
        asms.push_back(
            new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                               new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
    }
    break;
  case IRItem::RETURN:
    if (ir->items[0]->type == IRItem::ITEMP) {
      if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
        loadOrStoreFromSP(asms, false, ASMItem::A1,
                          spillOffsets[ir->items[0]->iVal]);
      else
        asms.push_back(
            new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                               new ASMItem(ASMItem::A1)}));
    } else {
      if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
        loadOrStoreFromSP(asms, false, ASMItem::S0,
                          spillOffsets[ir->items[0]->iVal]);
      else
        asms.push_back(
            new ASM(ASM::VMOV, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                                new ASMItem(ASMItem::S0)}));
    }
    break;
  case IRItem::SYMBOL:
    switch (ir->items[1]->symbol->symbolType) {
    case Symbol::CONST:
    case Symbol::GLOBAL_VAR:
      if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end()) {
        asms.push_back(
            new ASM(ASM::MOVW,
                    {new ASMItem(ASMItem::A1),
                     new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::MOVT,
                    {new ASMItem(ASMItem::A1),
                     new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
        loadOrStoreFromSP(asms, false, ASMItem::A1,
                          spillOffsets[ir->items[0]->iVal]);
      } else {
        asms.push_back(
            new ASM(ASM::MOVW,
                    {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                     new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::MOVT,
                    {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                     new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      }
      break;
    case Symbol::LOCAL_VAR:
    case Symbol::PARAM:
      if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end()) {
        moveFromSP(asms, ASMItem::A1, offsets[ir->items[1]->symbol]);
        loadOrStoreFromSP(asms, false, ASMItem::A1,
                          spillOffsets[ir->items[0]->iVal]);
      } else
        moveFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                   offsets[ir->items[1]->symbol]);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseNeg(vector<ASM *> &asms, IR *ir) {
  if (ir->items[0]->type == IRItem::ITEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadOrStoreFromSP(asms, true, ASMItem::A2,
                        spillOffsets[ir->items[1]->iVal]);
    asms.push_back(new ASM(
        ASM::RSB,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(0)}));
    if (flag1)
      loadOrStoreFromSP(asms, false, ASMItem::A1,
                        spillOffsets[ir->items[0]->iVal]);
  } else {
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadOrStoreFromSP(asms, true, ASMItem::S1,
                        spillOffsets[ir->items[1]->iVal]);
    asms.push_back(new ASM(
        ASM::VNEG,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal])}));
    if (flag1)
      loadOrStoreFromSP(asms, false, ASMItem::S0,
                        spillOffsets[ir->items[0]->iVal]);
  }
}

void ASMParser::parseReturn(vector<ASM *> &asms, IR *ir, IR *lastIR) {
  if (!ir->items.empty()) {
    if (ir->items[0]->type == IRItem::ITEMP)
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(itemp2Reg[ir->items[0]->iVal])}));
    else
      asms.push_back(
          new ASM(ASM::VMOV, {new ASMItem(ASMItem::S0),
                              new ASMItem(ftemp2Reg[ir->items[0]->iVal])}));
  }
  asms.push_back(
      new ASM(ASM::B, {new ASMItem(ASMItem::LABEL, irLabels[lastIR])}));
}

void ASMParser::parseStore(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end();
  if (flag1)
    loadOrStoreFromSP(asms, true, ASMItem::A1,
                      spillOffsets[ir->items[0]->iVal]);
  if (ir->items[1]->type == IRItem::ITEMP) {
    bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadOrStoreFromSP(asms, true, ASMItem::A2,
                        spillOffsets[ir->items[1]->iVal]);
    asms.push_back(new ASM(
        ASM::STR,
        {new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal])}));
  } else {
    bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadOrStoreFromSP(asms, true, ASMItem::A2,
                        spillOffsets[ir->items[1]->iVal]);
    asms.push_back(new ASM(
        flag2 ? ASM::STR : ASM::VSTR,
        {new ASMItem(flag2 ? ASMItem::A2 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal])}));
  }
}

void ASMParser::popArgs(vector<ASM *> &asms) {
  ASM *tempASM = new ASM(ASM::VPOP, {});
  for (unsigned i = 0; i < usedFRegs; i++)
    tempASM->items.push_back(new ASMItem(vFRegs[i]));
  if (tempASM->items.empty())
    delete tempASM;
  else
    asms.push_back(tempASM);
  tempASM = new ASM(ASM::POP, {});
  for (unsigned i = 0; i < usedIRegs; i++)
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
    vector<string> labels;
    for (IR *ir : funcIR.second)
      irLabels[ir] = labelId++;
  }
}

void ASMParser::removeUnusedLabels(vector<ASM *> &asms) {
  unordered_set<int> usedLabels;
  for (ASM *a : asms)
    if (a->type != ASM::LABEL)
      for (ASMItem *item : a->items)
        if (item->type == ASMItem::LABEL)
          usedLabels.insert(item->iVal);
  int size = 0;
  for (ASM *a : asms)
    if (a->type == ASM::LABEL &&
        usedLabels.find(a->items[0]->iVal) == usedLabels.end())
      delete a;
    else
      asms[size++] = a;
  asms.resize(size);
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
        offsets[func->params[i]] = offset + (usedIRegs + usedFRegs + 1) * 4;
        offset += 4;
      }
    } else {
      if (fCnt < 16) {
        asms.push_back(new ASM(ASM::VPUSH, {new ASMItem(aFRegs[fCnt++])}));
        offsets[func->params[i]] = -4 * (min(iCnt, 4u) + min(fCnt, 16u));
        savedRegs++;
      } else {
        offsets[func->params[i]] = offset + (usedIRegs + usedFRegs + 1) * 4;
        offset += 4;
      }
    }
  }
}

void ASMParser::saveUsedRegs(vector<ASM *> &asms) {
  ASM *tempASM = new ASM(ASM::PUSH, {});
  for (unsigned i = 0; i < usedIRegs; i++)
    tempASM->items.push_back(new ASMItem(vIRegs[i]));
  tempASM->items.push_back(new ASMItem(ASMItem::LR));
  asms.push_back(tempASM);
  tempASM = new ASM(ASM::VPUSH, {});
  for (unsigned i = 0; i < usedFRegs; i++)
    tempASM->items.push_back(new ASMItem(vFRegs[i]));
  if (tempASM->items.empty())
    delete tempASM;
  else
    asms.push_back(tempASM);
}