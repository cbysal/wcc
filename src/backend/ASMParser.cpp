#include <iostream>
#include <unordered_map>
#include <vector>

#include "ASMParser.h"

using namespace std;

#define MAX_IMM8 0xff
#define MAX_IMM16 0xffff

ASMParser::ASMParser(string &asmFile,
                     vector<pair<Symbol *, vector<IR *>>> &funcIRs,
                     vector<Symbol *> &consts, vector<Symbol *> &globalVars,
                     unordered_map<Symbol *, vector<Symbol *>> &localVars) {
  this->labelId = 0;
  this->asmFile = asmFile;
  this->funcIRs = funcIRs;
  this->consts = consts;
  this->globalVars = globalVars;
  this->localVars = localVars;
}

ASMParser::~ASMParser() {}

unordered_map<int, unsigned> ASMParser::allocReg(const vector<IR *> &irs) {
  unordered_map<int, unsigned> table;
  for (unsigned i = 0; i < irs.size(); i++)
    for (IRItem *item : irs[i]->items)
      if (item->type == IRItem::TEMP)
        table[item->iVal] = i;
  return table;
}

ASMItem::RegType ASMParser::getReg(int tempId) {
  const vector<ASMItem::RegType> regs = {ASMItem::V1, ASMItem::V2, ASMItem::V3,
                                         ASMItem::V4, ASMItem::V5, ASMItem::V6};
  if (temp2Reg.find(tempId) != temp2Reg.end())
    return temp2Reg[tempId];
  for (ASMItem::RegType reg : regs)
    if (reg2Temp.find(reg) == reg2Temp.end()) {
      reg2Temp[reg] = tempId;
      temp2Reg[tempId] = reg;
      return reg;
    }
  cerr << "Regfile not enough!" << endl;
  exit(-1);
  return ASMItem::PC;
}

vector<ASM *> ASMParser::parseFunc(Symbol *symbol, const vector<IR *> &irs) {
  reg2Temp.clear();
  temp2Reg.clear();
  unordered_map<int, unsigned> regEnds = allocReg(irs);
  vector<ASM *> asms;
  asms.push_back(
      new ASM(ASM::PUSH, {new ASMItem(ASMItem::V1), new ASMItem(ASMItem::V2),
                          new ASMItem(ASMItem::V3), new ASMItem(ASMItem::V4),
                          new ASMItem(ASMItem::V5), new ASMItem(ASMItem::V6),
                          new ASMItem(ASMItem::FP), new ASMItem(ASMItem::LR)}));
  asms.push_back(
      new ASM(ASM::ADD, {new ASMItem(ASMItem::FP), new ASMItem(ASMItem::SP),
                         new ASMItem(24)}));
  int top4Param = 0;
  switch (symbol->params.size()) {
  case 0:
    top4Param = 0;
    break;
  case 1:
    top4Param = 1;
    asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
    break;
  case 2:
    top4Param = 2;
    asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
    asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A2)}));
    break;
  case 3:
    top4Param = 3;
    asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
    asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A2)}));
    asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A3)}));
    break;
  default:
    top4Param = 4;
    asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
    asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A2)}));
    asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A3)}));
    asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A4)}));
    break;
  }
  unordered_map<Symbol *, int> offsets;
  for (unsigned i = 0; i < symbol->params.size(); i++)
    offsets[symbol->params[i]] = i < 4 ? -i * 4 - 28 : i * 4 - 8;
  vector<Symbol *> &localVarSymbols = localVars[symbol];
  int localVarOffset = 0;
  for (Symbol *localVarSymbol : localVarSymbols) {
    if (localVarSymbol->dimensions.empty())
      localVarOffset += 4;
    else {
      int size = 1;
      for (int dimension : localVarSymbol->dimensions)
        size *= dimension;
      localVarOffset += size * 4;
    }
    offsets[localVarSymbol] = -localVarOffset - top4Param * 4 - 24;
  }
  for (int i = 0; i < localVarOffset / MAX_IMM8; i++)
    asms.push_back(
        new ASM(ASM::SUB, {new ASMItem(ASMItem::SP), new ASMItem(ASMItem::SP),
                           new ASMItem(MAX_IMM8)}));
  asms.push_back(
      new ASM(ASM::SUB, {new ASMItem(ASMItem::SP), new ASMItem(ASMItem::SP),
                         new ASMItem(localVarOffset % MAX_IMM8)}));
  for (unsigned i = 0; i < irs.size(); i++) {
    asms.push_back(new ASM(ASM::LABEL, {new ASMItem(irLabels[irs[i]])}));
    ASMItem::RegType reg;
    switch (irs[i]->type) {
    case IR::ADD:
      reg = getReg(irs[i]->items[0]->iVal);
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(reg),
                             new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                             new ASMItem(temp2Reg[irs[i]->items[2]->iVal])}));
      break;
    case IR::ARG:
      asms.push_back(
          new ASM(ASM::PUSH, {new ASMItem(temp2Reg[irs[i]->items[0]->iVal])}));
      break;
    case IR::BEQ:
      if (irs[i]->items[2]->type == IRItem::TEMP)
        asms.push_back(
            new ASM(ASM::CMP, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                               new ASMItem(temp2Reg[irs[i]->items[2]->iVal])}));
      else
        asms.push_back(
            new ASM(ASM::CMP, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                               new ASMItem(irs[i]->items[2]->iVal)}));
      asms.push_back(new ASM(ASM::B, ASM::EQ,
                             {new ASMItem(irLabels[irs[i]->items[0]->ir])}));
      break;
    case IR::BGE:
      if (irs[i]->items[2]->type == IRItem::TEMP)
        asms.push_back(
            new ASM(ASM::CMP, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                               new ASMItem(temp2Reg[irs[i]->items[2]->iVal])}));
      else
        asms.push_back(
            new ASM(ASM::CMP, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                               new ASMItem(irs[i]->items[2]->iVal)}));
      asms.push_back(new ASM(ASM::B, ASM::GE,
                             {new ASMItem(irLabels[irs[i]->items[0]->ir])}));
      break;
    case IR::BGT:
      if (irs[i]->items[2]->type == IRItem::TEMP)
        asms.push_back(
            new ASM(ASM::CMP, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                               new ASMItem(temp2Reg[irs[i]->items[2]->iVal])}));
      else
        asms.push_back(
            new ASM(ASM::CMP, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                               new ASMItem(irs[i]->items[2]->iVal)}));
      asms.push_back(new ASM(ASM::B, ASM::GT,
                             {new ASMItem(irLabels[irs[i]->items[0]->ir])}));
      break;
    case IR::BLE:
      if (irs[i]->items[2]->type == IRItem::TEMP)
        asms.push_back(
            new ASM(ASM::CMP, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                               new ASMItem(temp2Reg[irs[i]->items[2]->iVal])}));
      else
        asms.push_back(
            new ASM(ASM::CMP, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                               new ASMItem(irs[i]->items[2]->iVal)}));
      asms.push_back(new ASM(ASM::B, ASM::LE,
                             {new ASMItem(irLabels[irs[i]->items[0]->ir])}));
      break;
    case IR::BLT:
      if (irs[i]->items[2]->type == IRItem::TEMP)
        asms.push_back(
            new ASM(ASM::CMP, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                               new ASMItem(temp2Reg[irs[i]->items[2]->iVal])}));
      else
        asms.push_back(
            new ASM(ASM::CMP, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                               new ASMItem(irs[i]->items[2]->iVal)}));
      asms.push_back(new ASM(ASM::B, ASM::LT,
                             {new ASMItem(irLabels[irs[i]->items[0]->ir])}));
      break;
    case IR::BNE:
      if (irs[i]->items[2]->type == IRItem::TEMP)
        asms.push_back(
            new ASM(ASM::CMP, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                               new ASMItem(temp2Reg[irs[i]->items[2]->iVal])}));
      else
        asms.push_back(
            new ASM(ASM::CMP, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                               new ASMItem(irs[i]->items[2]->iVal)}));
      asms.push_back(new ASM(ASM::B, ASM::NE,
                             {new ASMItem(irLabels[irs[i]->items[0]->ir])}));
      break;
    case IR::CALL:
      switch (irs[i]->items[0]->symbol ? irs[i]->items[0]->symbol->params.size()
                                       : irs[i]->items[0]->iVal) {
      case 0:
        break;
      case 1:
        asms.push_back(new ASM(ASM::POP, {new ASMItem(ASMItem::A1)}));
        break;
      case 2:
        asms.push_back(new ASM(
            ASM::POP, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A2)}));
        break;
      case 3:
        asms.push_back(new ASM(ASM::POP, {new ASMItem(ASMItem::A1),
                                          new ASMItem(ASMItem::A2),
                                          new ASMItem(ASMItem::A3)}));
        break;
      default:
        asms.push_back(new ASM(
            ASM::POP, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A2),
                       new ASMItem(ASMItem::A3), new ASMItem(ASMItem::A4)}));
        break;
      }
      asms.push_back(new ASM(
          ASM::BL,
          {new ASMItem(irs[i]->items[0]->symbol ? irs[i]->items[0]->symbol->name
                                                : irs[i]->items[0]->name)}));
      if ((irs[i]->items[0]->symbol ? irs[i]->items[0]->symbol->params.size()
                                    : irs[i]->items[0]->iVal) > 4)
        asms.push_back(
            new ASM(ASM::ADD,
                    {new ASMItem(ASMItem::SP), new ASMItem(ASMItem::SP),
                     new ASMItem(((irs[i]->items[0]->symbol
                                       ? irs[i]->items[0]->symbol->params.size()
                                       : irs[i]->items[0]->iVal) -
                                  4) *
                                 4)}));
      break;
    case IR::DIV:
      reg = getReg(irs[i]->items[0]->iVal);
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[irs[i]->items[1]->iVal])}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A2),
                             new ASMItem(temp2Reg[irs[i]->items[2]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_idiv")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
      break;
    case IR::FUNC_END:
      asms.push_back(
          new ASM(ASM::SUB, {new ASMItem(ASMItem::SP), new ASMItem(ASMItem::FP),
                             new ASMItem(24)}));
      asms.push_back(new ASM(
          ASM::POP, {new ASMItem(ASMItem::V1), new ASMItem(ASMItem::V2),
                     new ASMItem(ASMItem::V3), new ASMItem(ASMItem::V4),
                     new ASMItem(ASMItem::V5), new ASMItem(ASMItem::V6),
                     new ASMItem(ASMItem::FP), new ASMItem(ASMItem::PC)}));
      break;
    case IR::GOTO:
      asms.push_back(
          new ASM(ASM::B, {new ASMItem(irLabels[irs[i]->items[0]->ir])}));
      break;
    case IR::LOAD:
      reg = getReg(irs[i]->items[0]->iVal);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(reg),
                             new ASMItem(temp2Reg[irs[i]->items[1]->iVal])}));
      break;
    case IR::MEMSET_ZERO: {
      if (-offsets[irs[i]->items[0]->symbol] > MAX_IMM8) {
        asms.push_back(new ASM(ASM::SUB, {new ASMItem(ASMItem::A1),
                                          new ASMItem(ASMItem::FP),
                                          new ASMItem(MAX_IMM8)}));
        for (int j = 1; j < (-offsets[irs[i]->items[0]->symbol]) / MAX_IMM8;
             j++)
          asms.push_back(new ASM(ASM::SUB, {new ASMItem(ASMItem::A1),
                                            new ASMItem(ASMItem::A1),
                                            new ASMItem(MAX_IMM8)}));
        asms.push_back(new ASM(
            ASM::SUB,
            {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A1),
             new ASMItem((-offsets[irs[i]->items[0]->symbol]) % MAX_IMM8)}));
      } else {
        asms.push_back(new ASM(
            ASM::SUB,
            {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::FP),
             new ASMItem((-offsets[irs[i]->items[0]->symbol]) % MAX_IMM8)}));
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
      reg = getReg(irs[i]->items[0]->iVal);
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[irs[i]->items[1]->iVal])}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A2),
                             new ASMItem(temp2Reg[irs[i]->items[2]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_idivmod")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A2)}));
      break;
    case IR::MOV:
      reg = getReg(irs[i]->items[0]->iVal);
      switch (irs[i]->items[1]->type) {
      case IRItem::INT:
        if (irs[i]->items[1]->iVal < 0 || irs[i]->items[1]->iVal > MAX_IMM16) {
          asms.push_back(new ASM(
              ASM::MOV,
              {new ASMItem(reg),
               new ASMItem((unsigned)irs[i]->items[1]->iVal % 0x10000)}));
          asms.push_back(new ASM(
              ASM::MOVT,
              {new ASMItem(reg),
               new ASMItem((unsigned)irs[i]->items[1]->iVal / 0x10000)}));
        } else
          asms.push_back(
              new ASM(ASM::MOV,
                      {new ASMItem(reg), new ASMItem(irs[i]->items[1]->iVal)}));
        break;
      case IRItem::RETURN:
        asms.push_back(
            new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
        break;
      case IRItem::SYMBOL:
        switch (irs[i]->items[1]->symbol->symbolType) {
        case Symbol::CONST:
        case Symbol::GLOBAL_VAR:
          asms.push_back(
              new ASM(ASM::LDR, {new ASMItem(reg),
                                 new ASMItem("l" + to_string(labelId))}));
          asms.push_back(
              new ASM(ASM::LABEL, {new ASMItem("l" + to_string(labelId + 1))}));
          asms.push_back(
              new ASM(ASM::ADD, {new ASMItem(reg), new ASMItem(ASMItem::PC),
                                 new ASMItem(reg)}));
          asms.push_back(new ASM(ASM::B, {new ASMItem(irLabels[irs[i + 1]])}));
          asms.push_back(
              new ASM(ASM::LABEL, {new ASMItem("l" + to_string(labelId++))}));
          asms.push_back(
              new ASM(ASM::TAG, {new ASMItem(irs[i]->items[1]->symbol->name),
                                 new ASMItem("l" + to_string(labelId++))}));
          break;
        case Symbol::LOCAL_VAR:
        case Symbol::PARAM:
          if (offsets[irs[i]->items[1]->symbol] >= 0) {
            if (offsets[irs[i]->items[1]->symbol] > MAX_IMM8) {
              asms.push_back(
                  new ASM(ASM::ADD, {new ASMItem(reg), new ASMItem(ASMItem::FP),
                                     new ASMItem(MAX_IMM8)}));
              for (int j = 1; j < offsets[irs[i]->items[1]->symbol] / MAX_IMM8;
                   j++)
                asms.push_back(
                    new ASM(ASM::ADD, {new ASMItem(reg), new ASMItem(reg),
                                       new ASMItem(MAX_IMM8)}));
              asms.push_back(new ASM(
                  ASM::ADD,
                  {new ASMItem(reg), new ASMItem(reg),
                   new ASMItem(offsets[irs[i]->items[1]->symbol] % MAX_IMM8)}));
            } else {
              asms.push_back(new ASM(
                  ASM::ADD,
                  {new ASMItem(reg), new ASMItem(ASMItem::FP),
                   new ASMItem(offsets[irs[i]->items[1]->symbol] % MAX_IMM8)}));
            }
          } else {
            if (-offsets[irs[i]->items[1]->symbol] > MAX_IMM8) {
              asms.push_back(
                  new ASM(ASM::SUB, {new ASMItem(reg), new ASMItem(ASMItem::FP),
                                     new ASMItem(MAX_IMM8)}));
              for (int j = 1;
                   j < (-offsets[irs[i]->items[1]->symbol]) / MAX_IMM8; j++)
                asms.push_back(
                    new ASM(ASM::SUB, {new ASMItem(reg), new ASMItem(reg),
                                       new ASMItem(MAX_IMM8)}));
              asms.push_back(new ASM(
                  ASM::SUB, {new ASMItem(reg), new ASMItem(reg),
                             new ASMItem((-offsets[irs[i]->items[1]->symbol]) %
                                         MAX_IMM8)}));
            } else {
              asms.push_back(new ASM(
                  ASM::SUB, {new ASMItem(reg), new ASMItem(ASMItem::FP),
                             new ASMItem((-offsets[irs[i]->items[1]->symbol]) %
                                         MAX_IMM8)}));
            }
          }
          break;
        default:
          break;
        }
        break;
      case IRItem::TEMP:
        asms.push_back(
            new ASM(ASM::MOV, {new ASMItem(reg),
                               new ASMItem(temp2Reg[irs[i]->items[1]->iVal])}));
        break;
      default:
        break;
      }
      break;
    case IR::MUL:
      reg = getReg(irs[i]->items[0]->iVal);
      asms.push_back(
          new ASM(ASM::MUL, {new ASMItem(reg),
                             new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                             new ASMItem(temp2Reg[irs[i]->items[2]->iVal])}));
      break;
    case IR::NEG:
      reg = getReg(irs[i]->items[0]->iVal);
      asms.push_back(
          new ASM(ASM::RSB, {new ASMItem(reg),
                             new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                             new ASMItem(0)}));
      break;
    case IR::RETURN:
      if (!irs[i]->items.empty())
        asms.push_back(
            new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                               new ASMItem(temp2Reg[irs[i]->items[0]->iVal])}));
      asms.push_back(new ASM(ASM::B, {new ASMItem(irLabels[irs.back()])}));
      break;
    case IR::STORE:
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                             new ASMItem(temp2Reg[irs[i]->items[0]->iVal])}));
      break;
    case IR::SUB:
      reg = getReg(irs[i]->items[0]->iVal);
      asms.push_back(
          new ASM(ASM::SUB, {new ASMItem(reg),
                             new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                             new ASMItem(temp2Reg[irs[i]->items[2]->iVal])}));
      break;
    default:
      break;
    }
    vector<pair<ASMItem::RegType, unsigned>> toRemoveRegs;
    for (const pair<ASMItem::RegType, unsigned> p : reg2Temp)
      if (i >= regEnds[p.second])
        toRemoveRegs.push_back(p);
    for (pair<ASMItem::RegType, unsigned> toRemoveReg : toRemoveRegs) {
      reg2Temp.erase(toRemoveReg.first);
      temp2Reg.erase(toRemoveReg.second);
    }
  }
  return asms;
}

void ASMParser::parse() {
  preProcess();
  for (const pair<Symbol *, vector<IR *>> &funcIR : funcIRs)
    funcASMs.emplace_back(funcIR.first, parseFunc(funcIR.first, funcIR.second));
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

void ASMParser::writeASMFile() {
  parse();
  FILE *file = fopen(asmFile.data(), "w");
  fprintf(file, "\t.arch armv7-a\n");
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
  fclose(file);
}