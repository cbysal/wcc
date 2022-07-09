#include <iostream>

#include "ASMParser.h"

using namespace std;

ASMParser::ASMParser( pair<unsigned, unsigned> lineno,
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

void ASMParser::makeFrame(vector<ASM *> &asms, const vector<IR *> &irs,
                          Symbol *func) {
  allocReg(irs);
  int callArgSize = calcCallArgSize(irs);
  for (unordered_map<unsigned, int>::iterator it = temp2SpillReg.begin();
       it != temp2SpillReg.end(); it++)
    spillOffsets[it->first] = callArgSize + it->second * 4;
  pushArgs(asms);
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
  unsigned offset = frameOffset - savedRegs * 4;
  if (offset <= MAX_IMM8 * 3) {
    for (unsigned i = 0; i < offset / MAX_IMM8; i++)
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

void ASMParser::parseAdd(vector<ASM *> &asms, IR *ir) {
  if (ir->items[0]->type == IRItem::ITEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
         flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
    if (flag2)
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    if (flag3)
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[2]->iVal])}));
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
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
         flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
    if (flag2)
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S1), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    if (flag3)
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S2), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[2]->iVal])}));
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
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end()) {
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
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end()) {
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
  unsigned iCnt = 0, fCnt = 0;
  for (unsigned j = 1; j < ir->items.size(); j++) {
    if (ir->items[j]->type == IRItem::ITEMP) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(aIRegs[iCnt++]),
                             new ASMItem(itemp2Reg[ir->items[j]->iVal])}));
    } else {
      asms.push_back(
          new ASM(ASM::VMOV, {new ASMItem(aFRegs[fCnt++]),
                              new ASMItem(ftemp2Reg[ir->items[j]->iVal])}));
    }
  }
  if (!ir->items[0]->symbol->name.compare("_sysy_starttime")) {
    asms.push_back(new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                                      new ASMItem(startLineno % ZERO16)}));
    if (startLineno >= ZERO16)
      asms.push_back(new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                                         new ASMItem(startLineno / ZERO16)}));
  }
  if (!ir->items[0]->symbol->name.compare("_sysy_stoptime")) {
    asms.push_back(new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                                      new ASMItem(stopLineno % ZERO16)}));
    if (startLineno >= ZERO16)
      asms.push_back(new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                                         new ASMItem(stopLineno / ZERO16)}));
  }
  asms.push_back(new ASM(ASM::BL, {new ASMItem(ir->items[0]->symbol->name)}));
}

void ASMParser::parseCmp(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP) {
    bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
         flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
    if (flag2)
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    if (flag3)
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    asms.push_back(new ASM(
        ASM::CMP,
        {new ASMItem(flag2 ? ASMItem::A1 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::A2 : itemp2Reg[ir->items[2]->iVal])}));
  } else {
    bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
         flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
    if (flag2)
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    if (flag3)
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S1), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    asms.push_back(new ASM(
        ASM::VCMP,
        {new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::S1 : ftemp2Reg[ir->items[2]->iVal])}));
    asms.push_back(new ASM(ASM::VMRS, {}));
  }
  if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end()) {
    switch (ir->type) {
    case IR::EQ:
      asms.push_back(new ASM(ASM::MOV, ASM::EQ,
                             {new ASMItem(ASMItem::A1), new ASMItem(1)}));
      asms.push_back(new ASM(ASM::MOV, ASM::NE,
                             {new ASMItem(ASMItem::A1), new ASMItem(0)}));
      break;
    case IR::GE:
      asms.push_back(new ASM(ASM::MOV, ASM::GE,
                             {new ASMItem(ASMItem::A1), new ASMItem(1)}));
      asms.push_back(new ASM(ASM::MOV, ASM::LT,
                             {new ASMItem(ASMItem::A1), new ASMItem(0)}));
      break;
    case IR::GT:
      asms.push_back(new ASM(ASM::MOV, ASM::GT,
                             {new ASMItem(ASMItem::A1), new ASMItem(1)}));
      asms.push_back(new ASM(ASM::MOV, ASM::LE,
                             {new ASMItem(ASMItem::A1), new ASMItem(0)}));
      break;
    case IR::LE:
      asms.push_back(new ASM(ASM::MOV, ASM::LE,
                             {new ASMItem(ASMItem::A1), new ASMItem(1)}));
      asms.push_back(new ASM(ASM::MOV, ASM::GT,
                             {new ASMItem(ASMItem::A1), new ASMItem(0)}));
      break;
    case IR::LT:
      asms.push_back(new ASM(ASM::MOV, ASM::LT,
                             {new ASMItem(ASMItem::A1), new ASMItem(1)}));
      asms.push_back(new ASM(ASM::MOV, ASM::GE,
                             {new ASMItem(ASMItem::A1), new ASMItem(0)}));
      break;
    case IR::NE:
      asms.push_back(new ASM(ASM::MOV, ASM::NE,
                             {new ASMItem(ASMItem::A1), new ASMItem(1)}));
      asms.push_back(new ASM(ASM::MOV, ASM::EQ,
                             {new ASMItem(ASMItem::A1), new ASMItem(0)}));
      break;
    default:
      break;
    }
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                           new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  } else {
    switch (ir->type) {
    case IR::EQ:
      asms.push_back(new ASM(
          ASM::MOV, ASM::EQ,
          {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(1)}));
      asms.push_back(new ASM(
          ASM::MOV, ASM::NE,
          {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(0)}));
      break;
    case IR::GE:
      asms.push_back(new ASM(
          ASM::MOV, ASM::GE,
          {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(1)}));
      asms.push_back(new ASM(
          ASM::MOV, ASM::LT,
          {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(0)}));
      break;
    case IR::GT:
      asms.push_back(new ASM(
          ASM::MOV, ASM::GT,
          {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(1)}));
      asms.push_back(new ASM(
          ASM::MOV, ASM::LE,
          {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(0)}));
      break;
    case IR::LE:
      asms.push_back(new ASM(
          ASM::MOV, ASM::LE,
          {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(1)}));
      asms.push_back(new ASM(
          ASM::MOV, ASM::GT,
          {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(0)}));
      break;
    case IR::LT:
      asms.push_back(new ASM(
          ASM::MOV, ASM::LT,
          {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(1)}));
      asms.push_back(new ASM(
          ASM::MOV, ASM::GE,
          {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(0)}));
      break;
    case IR::NE:
      asms.push_back(new ASM(
          ASM::MOV, ASM::NE,
          {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(1)}));
      asms.push_back(new ASM(
          ASM::MOV, ASM::EQ,
          {new ASMItem(itemp2Reg[ir->items[0]->iVal]), new ASMItem(0)}));
      break;
    default:
      break;
    }
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
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
         flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
    if (flag2)
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S1), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    if (flag3)
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S2), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[2]->iVal])}));
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
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (flag2)
    asms.push_back(
        new ASM(ASM::VLDR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::SP),
                            new ASMItem(spillOffsets[ir->items[0]->iVal])}));
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

vector<ASM *> ASMParser::parseFunc(Symbol *func, const vector<IR *> &irs) {
  vector<ASM *> asms;
  initFrame();
  makeFrame(asms, irs, func);
  for (unsigned i = 0; i < irs.size(); i++) {
    asms.push_back(
        new ASM(ASM::LABEL, {new ASMItem(ASMItem::LABEL, irLabels[irs[i]])}));
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
    case IR::FUNC_END:
      parseFuncEnd(asms, irs[i]);
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
      parseReturn(asms, irs[i], irs.back());
      break;
    case IR::STORE:
      parseStore(asms, irs[i]);
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

void ASMParser::parseFuncEnd(vector<ASM *> &asms, IR *ir) {
  if (frameOffset <= MAX_IMM8 * 3) {
    for (unsigned i = 0; i < frameOffset / MAX_IMM8; i++)
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::SP), new ASMItem(ASMItem::SP),
                             new ASMItem(MAX_IMM8)}));
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(ASMItem::SP), new ASMItem(ASMItem::SP),
                           new ASMItem(frameOffset % MAX_IMM8)}));
  } else {
    asms.push_back(new ASM(ASM::MOV, {new ASMItem(ASMItem::A2),
                                      new ASMItem(frameOffset % ZERO16)}));
    if (frameOffset > MAX_IMM16)
      asms.push_back(new ASM(ASM::MOVT, {new ASMItem(ASMItem::A2),
                                         new ASMItem(frameOffset / ZERO16)}));
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(ASMItem::SP), new ASMItem(ASMItem::SP),
                           new ASMItem(ASMItem::A2)}));
  }
  popArgs(asms);
}

void ASMParser::parseI2F(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag2)
    asms.push_back(
        new ASM(ASM::LDR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                           new ASMItem(spillOffsets[ir->items[1]->iVal])}));
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
  if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end()) {
    if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end())
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    else if (ir->items[1]->type == IRItem::ITEMP)
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
    else
      asms.push_back(
          new ASM(ASM::VMOV, {new ASMItem(ASMItem::A1),
                              new ASMItem(ftemp2Reg[ir->items[1]->iVal])}));
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
    if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end())
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    else if (ir->items[1]->type == IRItem::ITEMP)
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
    else
      asms.push_back(
          new ASM(ASM::VMOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                              new ASMItem(ftemp2Reg[ir->items[1]->iVal])}));
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
  bool flag1 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag1)
    asms.push_back(
        new ASM(ASM::LDR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                           new ASMItem(spillOffsets[ir->items[1]->iVal])}));
  if (ir->items[0]->type == IRItem::ITEMP) {
    bool flag2 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end();
    asms.push_back(new ASM(
        ASM::LDR,
        {new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[1]->iVal])}));
    if (flag2)
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  } else {
    bool flag2 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end();
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

void ASMParser::parseMemsetZero(vector<ASM *> &asms, IR *ir) {
  if (offsets[ir->items[0]->symbol] <= MAX_IMM8) {
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                           new ASMItem(offsets[ir->items[0]->symbol])}));
  } else if (offsets[ir->items[0]->symbol] <= MAX_IMM8 * 3) {
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                           new ASMItem(MAX_IMM8)}));
    for (int j = 1; j < offsets[ir->items[0]->symbol] / MAX_IMM8; j++)
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A1),
                             new ASMItem(MAX_IMM8)}));
    asms.push_back(new ASM(
        ASM::ADD, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A1),
                   new ASMItem(offsets[ir->items[0]->symbol] % MAX_IMM8)}));
  } else {
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(ASMItem::A2),
                   new ASMItem(offsets[ir->items[0]->symbol] % ZERO16)}));
    if (offsets[ir->items[0]->symbol] > MAX_IMM16)
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(ASMItem::A2),
                      new ASMItem(offsets[ir->items[0]->symbol] / ZERO16)}));
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                           new ASMItem(ASMItem::A2)}));
  }
  asms.push_back(new ASM(ASM::MOV, {new ASMItem(ASMItem::A2), new ASMItem(0)}));
  int size = 4;
  for (int dimension : ir->items[0]->symbol->dimensions)
    size *= dimension;
  if (size > MAX_IMM16) {
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(ASMItem::A3), new ASMItem(MAX_IMM16)}));
    for (int j = 1; j < (size - MAX_IMM16) / MAX_IMM8; j++)
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::A3),
                             new ASMItem(MAX_IMM8)}));
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::A3),
                           new ASMItem((size - MAX_IMM16) % MAX_IMM8)}));
  } else {
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(ASMItem::A3), new ASMItem(size)}));
  }
  asms.push_back(new ASM(ASM::BL, {new ASMItem("memset")}));
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
    if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end()) {
      if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end()) {
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
      if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end())
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
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end()) {
      if (ir->items[1]->iVal < 0 || ir->items[1]->iVal > MAX_IMM16) {
        asms.push_back(new ASM(
            ASM::MOV, {new ASMItem(ASMItem::A1),
                       new ASMItem((unsigned)ir->items[1]->iVal % ZERO16)}));
        asms.push_back(new ASM(
            ASM::MOVT, {new ASMItem(ASMItem::A1),
                        new ASMItem((unsigned)ir->items[1]->iVal / ZERO16)}));
      } else
        asms.push_back(new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                                          new ASMItem(ir->items[1]->iVal)}));
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[0]->iVal])}));
    } else {
      if (ir->items[1]->iVal < 0 || ir->items[1]->iVal > MAX_IMM16) {
        asms.push_back(new ASM(
            ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                       new ASMItem((unsigned)ir->items[1]->iVal % ZERO16)}));
        asms.push_back(new ASM(
            ASM::MOVT, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                        new ASMItem((unsigned)ir->items[1]->iVal / ZERO16)}));
      } else
        asms.push_back(
            new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                               new ASMItem(ir->items[1]->iVal)}));
    }
    break;
  case IRItem::ITEMP:
    if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end()) {
      if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end()) {
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
      if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end())
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
    switch (ir->items[1]->symbol->symbolType) {
    case Symbol::CONST:
    case Symbol::GLOBAL_VAR:
      if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end()) {
        asms.push_back(
            new ASM(ASM::LDR, {new ASMItem(ASMItem::A1),
                               new ASMItem(ASMItem::LABEL, labelId)}));
        asms.push_back(
            new ASM(ASM::LABEL, {new ASMItem(ASMItem::LABEL, labelId + 1)}));
        asms.push_back(new ASM(ASM::ADD, {new ASMItem(ASMItem::A1),
                                          new ASMItem(ASMItem::PC),
                                          new ASMItem(ASMItem::A1)}));
        asms.push_back(
            new ASM(ASM::B, {new ASMItem(ASMItem::LABEL, labelId + 2)}));
        asms.push_back(
            new ASM(ASM::LABEL, {new ASMItem(ASMItem::LABEL, labelId++)}));
        asms.push_back(
            new ASM(ASM::TAG, {new ASMItem(ir->items[1]->symbol->name),
                               new ASMItem(ASMItem::LABEL, labelId++)}));
        asms.push_back(
            new ASM(ASM::LABEL, {new ASMItem(ASMItem::LABEL, labelId++)}));
        asms.push_back(new ASM(
            ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                       new ASMItem(spillOffsets[ir->items[0]->iVal])}));
      } else {
        asms.push_back(
            new ASM(ASM::LDR, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                               new ASMItem(ASMItem::LABEL, labelId)}));
        asms.push_back(
            new ASM(ASM::LABEL, {new ASMItem(ASMItem::LABEL, labelId + 1)}));
        asms.push_back(
            new ASM(ASM::ADD, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                               new ASMItem(ASMItem::PC),
                               new ASMItem(itemp2Reg[ir->items[0]->iVal])}));
        asms.push_back(
            new ASM(ASM::B, {new ASMItem(ASMItem::LABEL, labelId + 2)}));
        asms.push_back(
            new ASM(ASM::LABEL, {new ASMItem(ASMItem::LABEL, labelId++)}));
        asms.push_back(
            new ASM(ASM::TAG, {new ASMItem(ir->items[1]->symbol->name),
                               new ASMItem(ASMItem::LABEL, labelId++)}));
        asms.push_back(
            new ASM(ASM::LABEL, {new ASMItem(ASMItem::LABEL, labelId++)}));
      }
      break;
    case Symbol::LOCAL_VAR:
    case Symbol::PARAM:
      if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end()) {
        if (offsets[ir->items[1]->symbol] > MAX_IMM8) {
          if (offsets[ir->items[1]->symbol] <= MAX_IMM8 * 3) {
            asms.push_back(new ASM(ASM::ADD, {new ASMItem(ASMItem::A1),
                                              new ASMItem(ASMItem::SP),
                                              new ASMItem(MAX_IMM8)}));
            for (int j = 1; j < offsets[ir->items[1]->symbol] / MAX_IMM8; j++)
              asms.push_back(new ASM(ASM::ADD, {new ASMItem(ASMItem::A1),
                                                new ASMItem(ASMItem::A1),
                                                new ASMItem(MAX_IMM8)}));
            asms.push_back(new ASM(
                ASM::ADD,
                {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A1),
                 new ASMItem(offsets[ir->items[1]->symbol] % MAX_IMM8)}));
          } else {
            asms.push_back(new ASM(
                ASM::MOV, {new ASMItem(ASMItem::A1),
                           new ASMItem((unsigned)offsets[ir->items[1]->symbol] %
                                       ZERO16)}));
            if (offsets[ir->items[1]->symbol] > MAX_IMM16)
              asms.push_back(
                  new ASM(ASM::MOVT,
                          {new ASMItem(ASMItem::A1),
                           new ASMItem((unsigned)offsets[ir->items[1]->symbol] /
                                       ZERO16)}));
            asms.push_back(new ASM(ASM::ADD, {new ASMItem(ASMItem::A1),
                                              new ASMItem(ASMItem::SP),
                                              new ASMItem(ASMItem::A1)}));
          }
        } else {
          asms.push_back(new ASM(
              ASM::ADD, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                         new ASMItem(offsets[ir->items[1]->symbol])}));
        }
        asms.push_back(new ASM(
            ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP),
                       new ASMItem(spillOffsets[ir->items[0]->iVal])}));
      } else {
        if (offsets[ir->items[1]->symbol] > MAX_IMM8) {
          if (offsets[ir->items[1]->symbol] <= MAX_IMM8 * 3) {
            asms.push_back(new ASM(
                ASM::ADD, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                           new ASMItem(ASMItem::SP), new ASMItem(MAX_IMM8)}));
            for (int j = 1; j < offsets[ir->items[1]->symbol] / MAX_IMM8; j++)
              asms.push_back(
                  new ASM(ASM::ADD, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                                     new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                                     new ASMItem(MAX_IMM8)}));
            asms.push_back(new ASM(
                ASM::ADD,
                {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                 new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                 new ASMItem(offsets[ir->items[1]->symbol] % MAX_IMM8)}));
          } else {
            asms.push_back(new ASM(
                ASM::MOV, {new ASMItem(ASMItem::A1),
                           new ASMItem((unsigned)offsets[ir->items[1]->symbol] %
                                       ZERO16)}));
            if (offsets[ir->items[1]->symbol] > MAX_IMM16)
              asms.push_back(
                  new ASM(ASM::MOVT,
                          {new ASMItem(ASMItem::A1),
                           new ASMItem((unsigned)offsets[ir->items[1]->symbol] /
                                       ZERO16)}));
            asms.push_back(
                new ASM(ASM::ADD,
                        {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                         new ASMItem(ASMItem::SP), new ASMItem(ASMItem::A1)}));
          }
        } else {
          asms.push_back(
              new ASM(ASM::ADD, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                                 new ASMItem(ASMItem::SP),
                                 new ASMItem(offsets[ir->items[1]->symbol])}));
        }
      }
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
  if (ir->items[0]->type == IRItem::ITEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
         flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
    if (flag2)
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    if (flag3)
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[2]->iVal])}));
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
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
         flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
    if (flag2)
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S1), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    if (flag3)
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S2), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[2]->iVal])}));
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
  if (ir->items[0]->type == IRItem::ITEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
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
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S1), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[1]->iVal])}));
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
    asms.push_back(
        new ASM(ASM::LDR, {new ASMItem(ASMItem::A1),
                           new ASMItem(spillOffsets[ir->items[0]->iVal])}));
  if (ir->items[1]->type == IRItem::ITEMP) {
    bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A2),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    asms.push_back(new ASM(
        ASM::STR,
        {new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal])}));
  } else {
    bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A2),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    asms.push_back(new ASM(
        flag2 ? ASM::STR : ASM::VSTR,
        {new ASMItem(flag2 ? ASMItem::A2 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal])}));
  }
}

void ASMParser::parseSub(vector<ASM *> &asms, IR *ir) {
  if (ir->items[0]->type == IRItem::ITEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
         flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
    if (flag2)
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    if (flag3)
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::SP),
                             new ASMItem(spillOffsets[ir->items[2]->iVal])}));
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
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
         flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
    if (flag2)
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S1), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[1]->iVal])}));
    if (flag3)
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S2), new ASMItem(ASMItem::SP),
                      new ASMItem(spillOffsets[ir->items[2]->iVal])}));
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

void ASMParser::pushArgs(vector<ASM *> &asms) {
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

vector<pair<Symbol *, vector<ASM *>>> ASMParser::getFuncASMs() {
  if (!isProcessed)
    parse();
  return funcASMs;
}