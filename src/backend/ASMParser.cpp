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
  vector<unsigned> smartImm;
  for (unsigned mask : makeSmartImmMask(offset))
    smartImm.push_back(offset & mask);
  ASM::ASMOpType op = isFloatReg(target) ? ASM::VLDR : ASM::LDR;
  unsigned maxOffset = isFloatReg(target) ? 1020 : 4095;
  switch (smartImm.size()) {
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
    } else if (smartImm[1] <= maxOffset) {
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::SP),
                             new ASMItem(smartImm[0])}));
      asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::A4),
                                  new ASMItem(smartImm[1])}));
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

vector<unsigned> ASMParser::makeSmartImmMask(unsigned imm) {
  const static vector<pair<unsigned, vector<unsigned>>> masks = {
      {0x00000000, {}},
      {0x000000ff, {0x000000ff}},
      {0x000003fc, {0x000003fc}},
      {0x00000ff0, {0x00000ff0}},
      {0x00003fc0, {0x00003fc0}},
      {0x0000ff00, {0x0000ff00}},
      {0x0003fc00, {0x0003fc00}},
      {0x000ff000, {0x000ff000}},
      {0x003fc000, {0x003fc000}},
      {0x00ff0000, {0x00ff0000}},
      {0x03fc0000, {0x03fc0000}},
      {0x0ff00000, {0x0ff00000}},
      {0x3fc00000, {0x3fc00000}},
      {0xc000003f, {0xc000003f}},
      {0xf000000f, {0xf000000f}},
      {0xfc000003, {0xfc000003}},
      {0xff000000, {0xff000000}},
      {0x0000ffff, {0x000000ff, 0x0000ff00}},
      {0x0003fcff, {0x000000ff, 0x0003fc00}},
      {0x0003fffc, {0x000003fc, 0x0003fc00}},
      {0x000ff0ff, {0x000000ff, 0x000ff000}},
      {0x000ff3fc, {0x000003fc, 0x000ff000}},
      {0x000ffff0, {0x00000ff0, 0x000ff000}},
      {0x003fc0ff, {0x000000ff, 0x003fc000}},
      {0x003fc3fc, {0x000003fc, 0x003fc000}},
      {0x003fcff0, {0x00000ff0, 0x003fc000}},
      {0x003fffc0, {0x00003fc0, 0x003fc000}},
      {0x00ff00ff, {0x000000ff, 0x00ff0000}},
      {0x00ff03fc, {0x000003fc, 0x00ff0000}},
      {0x00ff0ff0, {0x00000ff0, 0x00ff0000}},
      {0x00ff3fc0, {0x00003fc0, 0x00ff0000}},
      {0x00ffff00, {0x0000ff00, 0x00ff0000}},
      {0x03fc00ff, {0x000000ff, 0x03fc0000}},
      {0x03fc03fc, {0x000003fc, 0x03fc0000}},
      {0x03fc0ff0, {0x00000ff0, 0x03fc0000}},
      {0x03fc3fc0, {0x00003fc0, 0x03fc0000}},
      {0x03fcff00, {0x0000ff00, 0x03fc0000}},
      {0x03fffc00, {0x0003fc00, 0x03fc0000}},
      {0x0ff000ff, {0x000000ff, 0x0ff00000}},
      {0x0ff003fc, {0x000003fc, 0x0ff00000}},
      {0x0ff00ff0, {0x00000ff0, 0x0ff00000}},
      {0x0ff03fc0, {0x00003fc0, 0x0ff00000}},
      {0x0ff0ff00, {0x0000ff00, 0x0ff00000}},
      {0x0ff3fc00, {0x0003fc00, 0x0ff00000}},
      {0x0ffff000, {0x000ff000, 0x0ff00000}},
      {0x3fc000ff, {0x000000ff, 0x3fc00000}},
      {0x3fc003fc, {0x000003fc, 0x3fc00000}},
      {0x3fc00ff0, {0x00000ff0, 0x3fc00000}},
      {0x3fc03fc0, {0x00003fc0, 0x3fc00000}},
      {0x3fc0ff00, {0x0000ff00, 0x3fc00000}},
      {0x3fc3fc00, {0x0003fc00, 0x3fc00000}},
      {0x3fcff000, {0x000ff000, 0x3fc00000}},
      {0x3fffc000, {0x003fc000, 0x3fc00000}},
      {0xc0003fff, {0x00003fc0, 0xc000003f}},
      {0xc000ff3f, {0x0000ff00, 0xc000003f}},
      {0xc003fc3f, {0x0003fc00, 0xc000003f}},
      {0xc00ff03f, {0x000ff000, 0xc000003f}},
      {0xc03fc03f, {0x003fc000, 0xc000003f}},
      {0xc0ff003f, {0x00ff0000, 0xc000003f}},
      {0xc3fc003f, {0x03fc0000, 0xc000003f}},
      {0xcff0003f, {0x0ff00000, 0xc000003f}},
      {0xf0000fff, {0x00000ff0, 0xf000000f}},
      {0xf0003fcf, {0x00003fc0, 0xf000000f}},
      {0xf000ff0f, {0x0000ff00, 0xf000000f}},
      {0xf003fc0f, {0x0003fc00, 0xf000000f}},
      {0xf00ff00f, {0x000ff000, 0xf000000f}},
      {0xf03fc00f, {0x003fc000, 0xf000000f}},
      {0xf0ff000f, {0x00ff0000, 0xf000000f}},
      {0xf3fc000f, {0x03fc0000, 0xf000000f}},
      {0xfc0003ff, {0x000003fc, 0xfc000003}},
      {0xfc000ff3, {0x00000ff0, 0xfc000003}},
      {0xfc003fc3, {0x00003fc0, 0xfc000003}},
      {0xfc00ff03, {0x0000ff00, 0xfc000003}},
      {0xfc03fc03, {0x0003fc00, 0xfc000003}},
      {0xfc0ff003, {0x000ff000, 0xfc000003}},
      {0xfc3fc003, {0x003fc000, 0xfc000003}},
      {0xfcff0003, {0x00ff0000, 0xfc000003}},
      {0xff0000ff, {0x000000ff, 0xff000000}},
      {0xff0003fc, {0x000003fc, 0xff000000}},
      {0xff000ff0, {0x00000ff0, 0xff000000}},
      {0xff003fc0, {0x00003fc0, 0xff000000}},
      {0xff00ff00, {0x0000ff00, 0xff000000}},
      {0xff03fc00, {0x0003fc00, 0xff000000}},
      {0xff0ff000, {0x000ff000, 0xff000000}},
      {0xff3fc000, {0x003fc000, 0xff000000}},
      {0xffc0003f, {0x3fc00000, 0xc000003f}},
      {0xfff0000f, {0x0ff00000, 0xf000000f}},
      {0xfffc0003, {0x03fc0000, 0xfc000003}},
      {0xffff0000, {0x00ff0000, 0xff000000}},
      {0x00ffffff, {0x000000ff, 0x0000ff00, 0x00ff0000}},
      {0x03fcffff, {0x000000ff, 0x0000ff00, 0x03fc0000}},
      {0x03fffcff, {0x000000ff, 0x0003fc00, 0x03fc0000}},
      {0x03fffffc, {0x000003fc, 0x0003fc00, 0x03fc0000}},
      {0x0ff0ffff, {0x000000ff, 0x0000ff00, 0x0ff00000}},
      {0x0ff3fcff, {0x000000ff, 0x0003fc00, 0x0ff00000}},
      {0x0ff3fffc, {0x000003fc, 0x0003fc00, 0x0ff00000}},
      {0x0ffff0ff, {0x000000ff, 0x000ff000, 0x0ff00000}},
      {0x0ffff3fc, {0x000003fc, 0x000ff000, 0x0ff00000}},
      {0x0ffffff0, {0x00000ff0, 0x000ff000, 0x0ff00000}},
      {0x3fc0ffff, {0x000000ff, 0x0000ff00, 0x3fc00000}},
      {0x3fc3fcff, {0x000000ff, 0x0003fc00, 0x3fc00000}},
      {0x3fc3fffc, {0x000003fc, 0x0003fc00, 0x3fc00000}},
      {0x3fcff0ff, {0x000000ff, 0x000ff000, 0x3fc00000}},
      {0x3fcff3fc, {0x000003fc, 0x000ff000, 0x3fc00000}},
      {0x3fcffff0, {0x00000ff0, 0x000ff000, 0x3fc00000}},
      {0x3fffc0ff, {0x000000ff, 0x003fc000, 0x3fc00000}},
      {0x3fffc3fc, {0x000003fc, 0x003fc000, 0x3fc00000}},
      {0x3fffcff0, {0x00000ff0, 0x003fc000, 0x3fc00000}},
      {0x3fffffc0, {0x00003fc0, 0x003fc000, 0x3fc00000}},
      {0xc03fffff, {0x00003fc0, 0x003fc000, 0xc000003f}},
      {0xc0ff3fff, {0x00003fc0, 0x00ff0000, 0xc000003f}},
      {0xc0ffff3f, {0x0000ff00, 0x00ff0000, 0xc000003f}},
      {0xc3fc3fff, {0x00003fc0, 0x03fc0000, 0xc000003f}},
      {0xc3fcff3f, {0x0000ff00, 0x03fc0000, 0xc000003f}},
      {0xc3fffc3f, {0x0003fc00, 0x03fc0000, 0xc000003f}},
      {0xcff03fff, {0x00003fc0, 0x0ff00000, 0xc000003f}},
      {0xcff0ff3f, {0x0000ff00, 0x0ff00000, 0xc000003f}},
      {0xcff3fc3f, {0x0003fc00, 0x0ff00000, 0xc000003f}},
      {0xcffff03f, {0x000ff000, 0x0ff00000, 0xc000003f}},
      {0xf00fffff, {0x00000ff0, 0x000ff000, 0xf000000f}},
      {0xf03fcfff, {0x00000ff0, 0x003fc000, 0xf000000f}},
      {0xf03fffcf, {0x00003fc0, 0x003fc000, 0xf000000f}},
      {0xf0ff0fff, {0x00000ff0, 0x00ff0000, 0xf000000f}},
      {0xf0ff3fcf, {0x00003fc0, 0x00ff0000, 0xf000000f}},
      {0xf0ffff0f, {0x0000ff00, 0x00ff0000, 0xf000000f}},
      {0xf3fc0fff, {0x00000ff0, 0x03fc0000, 0xf000000f}},
      {0xf3fc3fcf, {0x00003fc0, 0x03fc0000, 0xf000000f}},
      {0xf3fcff0f, {0x0000ff00, 0x03fc0000, 0xf000000f}},
      {0xf3fffc0f, {0x0003fc00, 0x03fc0000, 0xf000000f}},
      {0xfc03ffff, {0x000003fc, 0x0003fc00, 0xfc000003}},
      {0xfc0ff3ff, {0x000003fc, 0x000ff000, 0xfc000003}},
      {0xfc0ffff3, {0x00000ff0, 0x000ff000, 0xfc000003}},
      {0xfc3fc3ff, {0x000003fc, 0x003fc000, 0xfc000003}},
      {0xfc3fcff3, {0x00000ff0, 0x003fc000, 0xfc000003}},
      {0xfc3fffc3, {0x00003fc0, 0x003fc000, 0xfc000003}},
      {0xfcff03ff, {0x000003fc, 0x00ff0000, 0xfc000003}},
      {0xfcff0ff3, {0x00000ff0, 0x00ff0000, 0xfc000003}},
      {0xfcff3fc3, {0x00003fc0, 0x00ff0000, 0xfc000003}},
      {0xfcffff03, {0x0000ff00, 0x00ff0000, 0xfc000003}},
      {0xff00ffff, {0x000000ff, 0x0000ff00, 0xff000000}},
      {0xff03fcff, {0x000000ff, 0x0003fc00, 0xff000000}},
      {0xff03fffc, {0x000003fc, 0x0003fc00, 0xff000000}},
      {0xff0ff0ff, {0x000000ff, 0x000ff000, 0xff000000}},
      {0xff0ff3fc, {0x000003fc, 0x000ff000, 0xff000000}},
      {0xff0ffff0, {0x00000ff0, 0x000ff000, 0xff000000}},
      {0xff3fc0ff, {0x000000ff, 0x003fc000, 0xff000000}},
      {0xff3fc3fc, {0x000003fc, 0x003fc000, 0xff000000}},
      {0xff3fcff0, {0x00000ff0, 0x003fc000, 0xff000000}},
      {0xff3fffc0, {0x00003fc0, 0x003fc000, 0xff000000}},
      {0xffc03fff, {0x00003fc0, 0x3fc00000, 0xc000003f}},
      {0xffc0ff3f, {0x0000ff00, 0x3fc00000, 0xc000003f}},
      {0xffc3fc3f, {0x0003fc00, 0x3fc00000, 0xc000003f}},
      {0xffcff03f, {0x000ff000, 0x3fc00000, 0xc000003f}},
      {0xfff00fff, {0x00000ff0, 0x0ff00000, 0xf000000f}},
      {0xfff03fcf, {0x00003fc0, 0x0ff00000, 0xf000000f}},
      {0xfff0ff0f, {0x0000ff00, 0x0ff00000, 0xf000000f}},
      {0xfff3fc0f, {0x0003fc00, 0x0ff00000, 0xf000000f}},
      {0xfffc03ff, {0x000003fc, 0x03fc0000, 0xfc000003}},
      {0xfffc0ff3, {0x00000ff0, 0x03fc0000, 0xfc000003}},
      {0xfffc3fc3, {0x00003fc0, 0x03fc0000, 0xfc000003}},
      {0xfffcff03, {0x0000ff00, 0x03fc0000, 0xfc000003}},
      {0xffff00ff, {0x000000ff, 0x00ff0000, 0xff000000}},
      {0xffff03fc, {0x000003fc, 0x00ff0000, 0xff000000}},
      {0xffff0ff0, {0x00000ff0, 0x00ff0000, 0xff000000}},
      {0xffff3fc0, {0x00003fc0, 0x00ff0000, 0xff000000}},
      {0xffffc03f, {0x003fc000, 0x3fc00000, 0xc000003f}},
      {0xfffff00f, {0x000ff000, 0x0ff00000, 0xf000000f}},
      {0xfffffc03, {0x0003fc00, 0x03fc0000, 0xfc000003}},
      {0xffffff00, {0x0000ff00, 0x00ff0000, 0xff000000}}};
  for (const pair<unsigned, vector<unsigned>> &mask : masks)
    if (!(imm & ~mask.first))
      return mask.second;
  return {0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000};
}

void ASMParser::moveFromSP(vector<ASM *> &asms, ASMItem::RegType target,
                           int offset) {
  ASM::ASMOpType op = offset > 0 ? ASM::ADD : ASM::SUB;
  offset = abs(offset);
  vector<unsigned> smartImm;
  for (unsigned mask : makeSmartImmMask(offset))
    smartImm.push_back(offset & mask);
  switch (smartImm.size()) {
  case 0:
    if (target != ASMItem::SP)
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(target), new ASMItem(ASMItem::SP)}));
    break;
  case 1:
    asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::SP),
                                new ASMItem(smartImm[0])}));
    break;
  case 2:
    if (!(offset & 0xffff0000)) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(offset)}));
      asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::SP),
                                  new ASMItem(ASMItem::A4)}));
    } else {
      asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(ASMItem::SP),
                                  new ASMItem(smartImm[0])}));
      asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(target),
                                  new ASMItem(smartImm[1])}));
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

void ASMParser::parseAdd(vector<ASM *> &asms, IR *ir) {
  if (ir->items[0]->type == IRItem::ITEMP &&
      ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
         flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadFromSP(asms, ASMItem::A3, spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        ASM::ADD,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::A3 : itemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[0]->type == IRItem::ITEMP &&
             ir->items[1]->type == IRItem::ITEMP &&
             ir->items[2]->type == IRItem::INT) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::A3, ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::ADD,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::A3)}));
    if (flag1)
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[0]->type == IRItem::FTEMP &&
             ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FTEMP) {
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
         flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S1, spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadFromSP(asms, ASMItem::S2, spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        ASM::VADD,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::S2 : ftemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      storeFromSP(asms, ASMItem::S0, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[0]->type == IRItem::FTEMP &&
             ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FLOAT) {
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S1, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::S2, ir->items[2]->fVal);
    asms.push_back(new ASM(
        ASM::VADD,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::S2)}));
    if (flag1)
      storeFromSP(asms, ASMItem::S0, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[0]->type == IRItem::RETURN &&
             ir->items[1]->type == IRItem::ITEMP &&
             ir->items[2]->type == IRItem::ITEMP) {
    bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
         flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadFromSP(asms, ASMItem::A3, spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        ASM::ADD,
        {new ASMItem(ASMItem::A1),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::A3 : itemp2Reg[ir->items[2]->iVal])}));
  } else if (ir->items[0]->type == IRItem::RETURN &&
             ir->items[1]->type == IRItem::ITEMP &&
             ir->items[2]->type == IRItem::INT) {
    bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::A3, ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::ADD,
        {new ASMItem(ASMItem::A1),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::A3)}));
  } else if (ir->items[0]->type == IRItem::RETURN &&
             ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FTEMP) {
    bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
         flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S1, spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadFromSP(asms, ASMItem::S2, spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        ASM::VADD,
        {new ASMItem(ASMItem::S0),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::S2 : ftemp2Reg[ir->items[2]->iVal])}));
  } else if (ir->items[0]->type == IRItem::RETURN &&
             ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FLOAT) {
    bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S1, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::S2, ir->items[2]->fVal);
    asms.push_back(new ASM(
        ASM::VADD,
        {new ASMItem(ASMItem::S0),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::S2)}));
  }
}

void ASMParser::parseB(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP) {
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
  } else if (ir->items[1]->type == IRItem::ITEMP &&
             ir->items[2]->type == IRItem::INT) {
    bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::A2, ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::CMP,
        {new ASMItem(flag2 ? ASMItem::A1 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::A2)}));
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FTEMP) {
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
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FLOAT) {
    bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S0, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::S1, ir->items[2]->fVal);
    asms.push_back(new ASM(
        ASM::VCMP,
        {new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::S1)}));
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
        loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[i]->iVal]);
        storeFromSP(asms, ASMItem::A1, offset);
      } else
        storeFromSP(asms, itemp2Reg[ir->items[i]->iVal], offset);
    } else if (ir->items[i]->type == IRItem::FTEMP) {
      if (fCnt < 16) {
        fCnt++;
        continue;
      }
      if (ftemp2Reg.find(ir->items[i]->iVal) == ftemp2Reg.end()) {
        loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[i]->iVal]);
        storeFromSP(asms, ASMItem::A1, offset);
      } else
        storeFromSP(asms, ftemp2Reg[ir->items[i]->iVal], offset);
    } else if (ir->items[i]->type == IRItem::INT) {
      if (iCnt < 4) {
        iCnt++;
        continue;
      }
      loadFromSP(asms, ASMItem::A1, ir->items[i]->iVal);
      storeFromSP(asms, ASMItem::A1, offset);
    } else if (ir->items[i]->type == IRItem::FLOAT) {
      if (iCnt < 4) {
        iCnt++;
        continue;
      }
      loadFromSP(asms, ASMItem::S0, ir->items[i]->fVal);
      storeFromSP(asms, ASMItem::S0, offset);
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
        loadImmToReg(asms, aIRegs[iCnt++], ir->items[i]->iVal);
    } else if (ir->items[i]->type == IRItem::FLOAT) {
      if (fCnt < 16)
        loadFromSP(asms, aFRegs[fCnt++], ir->items[i]->fVal);
    }
  }
  if (!ir->items[0]->symbol->name.compare("_sysy_starttime"))
    loadImmToReg(asms, ASMItem::A1, startLineno);
  if (!ir->items[0]->symbol->name.compare("_sysy_stoptime"))
    loadImmToReg(asms, ASMItem::A1, stopLineno);
  asms.push_back(new ASM(ASM::BL, {new ASMItem(ir->items[0]->symbol->name)}));
}

void ASMParser::parseCmp(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP) {
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
  } else if (ir->items[1]->type == IRItem::ITEMP &&
             ir->items[2]->type == IRItem::INT) {
    bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::A2, ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::CMP,
        {new ASMItem(flag2 ? ASMItem::A1 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::A2)}));
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FTEMP) {
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
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FLOAT) {
    bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S0, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::S1, ir->items[2]->fVal);
    asms.push_back(new ASM(
        ASM::VCMP,
        {new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::S1)}));
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
    storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseDiv(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
         flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadFromSP(asms, ASMItem::A3, spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        ASM::DIV,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::A3 : itemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[1]->type == IRItem::ITEMP &&
             ir->items[2]->type == IRItem::INT) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::A3, ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::DIV,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::A3)}));
    if (flag1)
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FTEMP) {
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
         flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S1, spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadFromSP(asms, ASMItem::S2, spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        ASM::VDIV,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::S2 : ftemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      storeFromSP(asms, ASMItem::S0, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FLOAT) {
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S1, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::S2, ir->items[2]->fVal);
    asms.push_back(new ASM(
        ASM::VDIV,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::S2)}));
    if (flag1)
      storeFromSP(asms, ASMItem::S0, spillOffsets[ir->items[0]->iVal]);
  }
}

void ASMParser::parseF2I(vector<ASM *> &asms, IR *ir) {
  switch (ir->items[0]->type) {
  case IRItem::ITEMP: {
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
    break;
  }
  case IRItem::RETURN: {
    bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S0, spillOffsets[ir->items[1]->iVal]);
    asms.push_back(new ASM(
        ASM::VCVTFS,
        {new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal])}));
    asms.push_back(new ASM(
        ASM::VMOV,
        {new ASMItem(ASMItem::A1),
         new ASMItem(flag2 ? ASMItem::S0 : ftemp2Reg[ir->items[1]->iVal])}));
    break;
  }
  default:
    break;
  }
}

vector<ASM *> ASMParser::parseFunc(Symbol *func, const vector<IR *> &irs) {
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
      switch (irs[i]->items[0]->type) {
      case IRItem::FTEMP:
        parseMovToFtemp(asms, irs[i]);
        break;
      case IRItem::ITEMP:
        parseMovToItemp(asms, irs[i]);
        break;
      case IRItem::RETURN:
        parseMovToReturn(asms, irs[i]);
        break;
      case IRItem::SYMBOL:
        parseMovToSymbol(asms, irs[i]);
        break;
      default:
        break;
      }
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
  moveFromSP(asms, ASMItem::SP, frameOffset);
  popArgs(asms);
  return asms;
}

void ASMParser::parseI2F(vector<ASM *> &asms, IR *ir) {
  switch (ir->items[0]->type) {
  case IRItem::FTEMP: {
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
    break;
  }
  case IRItem::RETURN: {
    bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S0, spillOffsets[ir->items[1]->iVal]);
    else
      asms.push_back(
          new ASM(ASM::VMOV, {new ASMItem(ASMItem::S0),
                              new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
    asms.push_back(new ASM(
        ASM::VCVTSF, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::S0)}));
    break;
  }
  default:
    break;
  }
}

void ASMParser::parseLNot(vector<ASM *> &asms, IR *ir) {
  if (ir->items[0]->type == IRItem::ITEMP &&
      ir->items[1]->type == IRItem::ITEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    ASMItem::RegType targetReg =
        flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal];
    if (flag2) {
      loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(
          new ASM(ASM::CMP, {new ASMItem(ASMItem::A1), new ASMItem(0)}));
    } else {
      asms.push_back(
          new ASM(ASM::CMP, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                             new ASMItem(0)}));
    }
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::NE, {new ASMItem(targetReg), new ASMItem(0)}));
    if (flag1)
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[0]->type == IRItem::ITEMP &&
             ir->items[1]->type == IRItem::FTEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    ASMItem::RegType targetReg =
        flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal];
    if (flag2) {
      loadFromSP(asms, ASMItem::S0, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(
          new ASM(ASM::VCMP, {new ASMItem(ASMItem::S0), new ASMItem(0.0f)}));
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
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[0]->type == IRItem::RETURN &&
             ir->items[1]->type == IRItem::ITEMP) {
    bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2) {
      loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(
          new ASM(ASM::CMP, {new ASMItem(ASMItem::A1), new ASMItem(0)}));
    } else {
      asms.push_back(
          new ASM(ASM::CMP, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                             new ASMItem(0)}));
    }
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ, {new ASMItem(ASMItem::A1), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::NE, {new ASMItem(ASMItem::A1), new ASMItem(0)}));
  } else if (ir->items[0]->type == IRItem::RETURN &&
             ir->items[1]->type == IRItem::FTEMP) {
    bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2) {
      loadFromSP(asms, ASMItem::S0, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(
          new ASM(ASM::VCMP, {new ASMItem(ASMItem::S0), new ASMItem(0.0f)}));
    } else {
      asms.push_back(
          new ASM(ASM::VCMP, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                              new ASMItem(0.0f)}));
    }
    asms.push_back(new ASM(ASM::VMRS, {}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ, {new ASMItem(ASMItem::A1), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::NE, {new ASMItem(ASMItem::A1), new ASMItem(0)}));
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

void ASMParser::parseMod(vector<ASM *> &asms, IR *ir) {
  if (ir->items[0]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
         flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadFromSP(asms, ASMItem::A3, spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        ASM::DIV,
        {new ASMItem(ASMItem::A1),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::A3 : itemp2Reg[ir->items[2]->iVal])}));
    asms.push_back(new ASM(
        ASM::MUL,
        {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A1),
         new ASMItem(flag3 ? ASMItem::A3 : itemp2Reg[ir->items[2]->iVal])}));
    asms.push_back(new ASM(
        ASM::SUB,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::A1)}));
    if (flag1)
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[0]->type == IRItem::ITEMP &&
             ir->items[2]->type == IRItem::INT) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::A3, ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::DIV,
        {new ASMItem(ASMItem::A1),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::A3)}));
    asms.push_back(
        new ASM(ASM::MUL, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A1),
                           new ASMItem(ASMItem::A3)}));
    asms.push_back(new ASM(
        ASM::SUB,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::A1)}));
    if (flag1)
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[0]->type == IRItem::RETURN &&
             ir->items[2]->type == IRItem::ITEMP) {
    bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
         flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadFromSP(asms, ASMItem::A3, spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        ASM::DIV,
        {new ASMItem(ASMItem::A1),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::A3 : itemp2Reg[ir->items[2]->iVal])}));
    asms.push_back(new ASM(
        ASM::MUL,
        {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A1),
         new ASMItem(flag3 ? ASMItem::A3 : itemp2Reg[ir->items[2]->iVal])}));
    asms.push_back(new ASM(
        ASM::SUB,
        {new ASMItem(ASMItem::A1),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::A1)}));
  } else if (ir->items[0]->type == IRItem::RETURN &&
             ir->items[2]->type == IRItem::INT) {
    bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::A3, ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::DIV,
        {new ASMItem(ASMItem::A1),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::A3)}));
    asms.push_back(
        new ASM(ASM::MUL, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A1),
                           new ASMItem(ASMItem::A3)}));
    asms.push_back(new ASM(
        ASM::SUB,
        {new ASMItem(ASMItem::A1),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::A1)}));
  }
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

void ASMParser::parseMovToReturn(vector<ASM *> &asms, IR *ir) {
  switch (ir->items[1]->type) {
  case IRItem::ITEMP:
    if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end())
      loadFromSP(asms, ASMItem::A1, spillOffsets[ir->items[1]->iVal]);
    else
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
    break;
  case IRItem::FTEMP:
    if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end())
      loadFromSP(asms, ASMItem::S0, spillOffsets[ir->items[1]->iVal]);
    else
      asms.push_back(
          new ASM(ASM::VMOV, {new ASMItem(ASMItem::S0),
                              new ASMItem(ftemp2Reg[ir->items[1]->iVal])}));
    break;
  case IRItem::INT:
    loadImmToReg(asms, ASMItem::A1, ir->items[1]->iVal);
    break;
  case IRItem::FLOAT:
    loadImmToReg(asms, ASMItem::S0, ir->items[1]->fVal);
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
    if (ir->items[1]->symbol->dataType == Symbol::INT)
      asms.push_back(new ASM(
          ASM::LDR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A2)}));
    else
      asms.push_back(new ASM(
          ASM::VLDR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::A2)}));
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
        new ASM(ASM::ADD, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::A2),
                           new ASMItem(ASMItem::A3)}));
  }
  switch (ir->items[1]->type) {
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
  case IRItem::INT:
  case IRItem::FLOAT:
    loadImmToReg(asms, ASMItem::A3, ir->items[1]->iVal);
    asms.push_back(new ASM(
        ASM::STR, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::A2)}));
    break;
  case IRItem::RETURN:
    if (ir->items[0]->symbol->dataType == Symbol::FLOAT)
      asms.push_back(new ASM(
          ASM::VSTR, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::A2)}));
    else
      asms.push_back(new ASM(
          ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A2)}));
    break;
  default:
    break;
  }
}

void ASMParser::parseMul(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
         flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadFromSP(asms, ASMItem::A3, spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        ASM::MUL,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::A3 : itemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[1]->type == IRItem::ITEMP &&
             ir->items[2]->type == IRItem::INT) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::A3, ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::MUL,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::A3)}));
    if (flag1)
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FTEMP) {
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
         flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S1, spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadFromSP(asms, ASMItem::S2, spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        ASM::VMUL,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::S2 : ftemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      storeFromSP(asms, ASMItem::S0, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FLOAT) {
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S1, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::S2, ir->items[2]->fVal);
    asms.push_back(new ASM(
        ASM::VMUL,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::S2)}));
    if (flag1)
      storeFromSP(asms, ASMItem::S0, spillOffsets[ir->items[0]->iVal]);
  }
}

void ASMParser::parseNeg(vector<ASM *> &asms, IR *ir) {
  if (ir->items[0]->type == IRItem::ITEMP &&
      ir->items[1]->type == IRItem::ITEMP) {
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
  } else if (ir->items[0]->type == IRItem::FTEMP &&
             ir->items[1]->type == IRItem::FTEMP) {
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
  } else if (ir->items[0]->type == IRItem::RETURN &&
             ir->items[1]->type == IRItem::ITEMP) {
    bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    asms.push_back(new ASM(
        ASM::RSB,
        {new ASMItem(ASMItem::A1),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(0)}));
  } else if (ir->items[0]->type == IRItem::RETURN &&
             ir->items[1]->type == IRItem::FTEMP) {
    bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S1, spillOffsets[ir->items[1]->iVal]);
    asms.push_back(new ASM(
        ASM::VNEG,
        {new ASMItem(ASMItem::S0),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal])}));
  }
}

void ASMParser::parseSub(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
         flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadFromSP(asms, ASMItem::A3, spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        ASM::SUB,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::A3 : itemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[1]->type == IRItem::ITEMP &&
             ir->items[2]->type == IRItem::INT) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::A2, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::A3, ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::SUB,
        {new ASMItem(flag1 ? ASMItem::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::A3)}));
    if (flag1)
      storeFromSP(asms, ASMItem::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FTEMP) {
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
         flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S1, spillOffsets[ir->items[1]->iVal]);
    if (flag3)
      loadFromSP(asms, ASMItem::S2, spillOffsets[ir->items[2]->iVal]);
    asms.push_back(new ASM(
        ASM::VSUB,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag3 ? ASMItem::S2 : ftemp2Reg[ir->items[2]->iVal])}));
    if (flag1)
      storeFromSP(asms, ASMItem::S0, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FLOAT) {
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, ASMItem::S1, spillOffsets[ir->items[1]->iVal]);
    loadImmToReg(asms, ASMItem::S2, ir->items[2]->fVal);
    asms.push_back(new ASM(
        ASM::VSUB,
        {new ASMItem(flag1 ? ASMItem::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? ASMItem::S1 : ftemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::S2)}));
    if (flag1)
      storeFromSP(asms, ASMItem::S0, spillOffsets[ir->items[0]->iVal]);
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
  vector<unsigned> smartImm;
  for (unsigned mask : makeSmartImmMask(offset))
    smartImm.push_back(offset & mask);
  ASM::ASMOpType op = isFloatReg(source) ? ASM::VSTR : ASM::STR;
  unsigned maxOffset = isFloatReg(source) ? 1020 : 4095;
  switch (smartImm.size()) {
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
    } else if (smartImm[1] <= maxOffset) {
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::SP),
                             new ASMItem(smartImm[0])}));
      asms.push_back(new ASM(op, {new ASMItem(source), new ASMItem(ASMItem::A4),
                                  new ASMItem(smartImm[1])}));
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
