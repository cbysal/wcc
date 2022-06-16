#include <iostream>
#include <unordered_map>
#include <vector>

#include "ASMParser.h"

using namespace std;

#define MAX_IMM8 0xff
#define MAX_IMM16 0xffff
#define ZERO16 0x10000

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

void ASMParser::allocReg(const vector<IR *> &irs) {
  regEnds.clear();
  for (unsigned i = 0; i < irs.size(); i++)
    for (IRItem *item : irs[i]->items)
      if (item->type == IRItem::FTEMP || item->type == IRItem::ITEMP)
        regEnds[item->iVal] = i;
}

ASMItem::RegType ASMParser::getSReg(int tempId) {
  const vector<ASMItem::RegType> regs = {
      ASMItem::S0,  ASMItem::S1,  ASMItem::S2,  ASMItem::S3,  ASMItem::S4,
      ASMItem::S5,  ASMItem::S6,  ASMItem::S7,  ASMItem::S8,  ASMItem::S9,
      ASMItem::S10, ASMItem::S11, ASMItem::S12, ASMItem::S13, ASMItem::S14,
      ASMItem::S15, ASMItem::S16, ASMItem::S17, ASMItem::S18, ASMItem::S19,
      ASMItem::S20, ASMItem::S21, ASMItem::S22, ASMItem::S23, ASMItem::S24,
      ASMItem::S25, ASMItem::S26, ASMItem::S27, ASMItem::S28, ASMItem::S29,
      ASMItem::S30, ASMItem::S31};
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

ASMItem::RegType ASMParser::getVReg(int tempId) {
  const vector<ASMItem::RegType> regs = {ASMItem::V1, ASMItem::V2, ASMItem::V3,
                                         ASMItem::V4, ASMItem::V5, ASMItem::V6,
                                         ASMItem::V7};
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

void ASMParser::parse() {
  preProcess();
  for (const pair<Symbol *, vector<IR *>> &funcIR : funcIRs) {
    vector<ASM *> asms = parseFunc(funcIR.first, funcIR.second);
    removeUnusedLabels(asms);
    funcASMs.emplace_back(funcIR.first, asms);
  }
}

void ASMParser::parseAdd(vector<ASM *> &asms, IR *ir) {
  ASMItem::RegType reg = getVReg(ir->items[0]->iVal);
  bool isFloat = false;
  if ((ir->items[1]->type == IRItem::INT ||
       ir->items[1]->type == IRItem::ITEMP) &&
      (ir->items[2]->type == IRItem::INT ||
       ir->items[2]->type == IRItem::ITEMP)) {
    isFloat = false;
    asms.push_back(new ASM(
        ASM::ADD, {new ASMItem(reg), new ASMItem(temp2Reg[ir->items[1]->iVal]),
                   new ASMItem(temp2Reg[ir->items[2]->iVal])}));
  } else {
    isFloat = true;
    switch (ir->items[1]->type) {
    case IRItem::FLOAT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(ir->items[1]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(reg),
                              new ASMItem(ir->items[1]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::FTEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::INT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(ir->items[1]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                              new ASMItem(ir->items[1]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    case IRItem::ITEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    default:
      break;
    }
    switch (ir->items[2]->type) {
    case IRItem::FLOAT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(ir->items[2]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(reg),
                              new ASMItem(ir->items[2]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::FTEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(temp2Reg[ir->items[2]->iVal])}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::INT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(ir->items[2]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                              new ASMItem(ir->items[2]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    case IRItem::ITEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[ir->items[2]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    default:
      break;
    }
    asms.push_back(new ASM(ASM::POP, {new ASMItem(ASMItem::A2)}));
    asms.push_back(new ASM(ASM::POP, {new ASMItem(ASMItem::A1)}));
    asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_fadd")}));
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
  }
  if (ir->items[0]->type == IRItem::ITEMP) {
    if (isFloat) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1), new ASMItem(reg)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_f2iz")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
    }
  } else {
    if (!isFloat) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1), new ASMItem(reg)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
    }
  }
}

void ASMParser::parseB(vector<ASM *> &asms, IR *ir) {
  if ((ir->items[1]->type == IRItem::INT ||
       ir->items[1]->type == IRItem::ITEMP) &&
      (ir->items[2]->type == IRItem::INT ||
       ir->items[2]->type == IRItem::ITEMP)) {
    asms.push_back(
        new ASM(ASM::CMP, {ir->items[1]->type == IRItem::INT
                               ? new ASMItem(ir->items[1]->iVal)
                               : new ASMItem(temp2Reg[ir->items[1]->iVal]),
                           ir->items[2]->type == IRItem::INT
                               ? new ASMItem(ir->items[2]->iVal)
                               : new ASMItem(temp2Reg[ir->items[2]->iVal])}));
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
    switch (ir->items[1]->type) {
    case IRItem::FLOAT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A3),
                             new ASMItem(ir->items[1]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A3),
                              new ASMItem(ir->items[1]->iVal / ZERO16)}));
      break;
    case IRItem::FTEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A3),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
      break;
    case IRItem::INT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(ir->items[1]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                              new ASMItem(ir->items[1]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(
          ASM::MOV, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::A1)}));
      break;
    case IRItem::ITEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(
          ASM::MOV, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::A1)}));
      break;
    default:
      break;
    }
    switch (ir->items[2]->type) {
    case IRItem::FLOAT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A4),
                             new ASMItem(ir->items[2]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A4),
                              new ASMItem(ir->items[2]->iVal / ZERO16)}));
      break;
    case IRItem::FTEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A4),
                             new ASMItem(temp2Reg[ir->items[2]->iVal])}));
      break;
    case IRItem::INT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(ir->items[2]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                              new ASMItem(ir->items[2]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(
          ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::A1)}));
      break;
    case IRItem::ITEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[ir->items[2]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(
          ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::A1)}));
      break;
    default:
      break;
    }
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A3)}));
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::A4)}));
    switch (ir->type) {
    case IR::BEQ:
      asms.push_back(new ASM(ASM::BL, {new ASMItem(" __aeabi_fcmpeq")}));
      break;
    case IR::BGE:
      asms.push_back(new ASM(ASM::BL, {new ASMItem(" __aeabi_fcmpge")}));
      break;
    case IR::BGT:
      asms.push_back(new ASM(ASM::BL, {new ASMItem(" __aeabi_fcmpgt")}));
      break;
    case IR::BLE:
      asms.push_back(new ASM(ASM::BL, {new ASMItem(" __aeabi_fcmple")}));
      break;
    case IR::BLT:
      asms.push_back(new ASM(ASM::BL, {new ASMItem(" __aeabi_fcmplt")}));
      break;
    case IR::BNE:
      asms.push_back(new ASM(ASM::BL, {new ASMItem(" __aeabi_fcmpun")}));
      break;
    default:
      break;
    }
    asms.push_back(
        new ASM(ASM::CMP, {new ASMItem(ASMItem::A1), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::B, ASM::EQ, {new ASMItem(irLabels[ir->items[0]->ir])}));
  }
}

void ASMParser::parseCmp(vector<ASM *> &asms, IR *ir) {
  ASMItem::RegType reg = getVReg(ir->items[0]->iVal);
  if ((ir->items[1]->type == IRItem::INT ||
       ir->items[1]->type == IRItem::ITEMP) &&
      (ir->items[2]->type == IRItem::INT ||
       ir->items[2]->type == IRItem::ITEMP)) {
    asms.push_back(
        new ASM(ASM::CMP, {ir->items[1]->type == IRItem::INT
                               ? new ASMItem(ir->items[1]->iVal)
                               : new ASMItem(temp2Reg[ir->items[1]->iVal]),
                           ir->items[2]->type == IRItem::INT
                               ? new ASMItem(ir->items[2]->iVal)
                               : new ASMItem(temp2Reg[ir->items[2]->iVal])}));
    switch (ir->type) {
    case IR::EQ:
      asms.push_back(
          new ASM(ASM::MOV, ASM::EQ, {new ASMItem(reg), new ASMItem(1)}));
      asms.push_back(
          new ASM(ASM::MOV, ASM::NE, {new ASMItem(reg), new ASMItem(0)}));
      break;
    case IR::GE:
      asms.push_back(
          new ASM(ASM::MOV, ASM::GE, {new ASMItem(reg), new ASMItem(1)}));
      asms.push_back(
          new ASM(ASM::MOV, ASM::LT, {new ASMItem(reg), new ASMItem(0)}));
      break;
    case IR::GT:
      asms.push_back(
          new ASM(ASM::MOV, ASM::GT, {new ASMItem(reg), new ASMItem(1)}));
      asms.push_back(
          new ASM(ASM::MOV, ASM::LE, {new ASMItem(reg), new ASMItem(0)}));
      break;
    case IR::LE:
      asms.push_back(
          new ASM(ASM::MOV, ASM::LE, {new ASMItem(reg), new ASMItem(1)}));
      asms.push_back(
          new ASM(ASM::MOV, ASM::GT, {new ASMItem(reg), new ASMItem(0)}));
      break;
    case IR::LT:
      asms.push_back(
          new ASM(ASM::MOV, ASM::LT, {new ASMItem(reg), new ASMItem(1)}));
      asms.push_back(
          new ASM(ASM::MOV, ASM::GE, {new ASMItem(reg), new ASMItem(0)}));
      break;
    case IR::NE:
      asms.push_back(
          new ASM(ASM::MOV, ASM::NE, {new ASMItem(reg), new ASMItem(1)}));
      asms.push_back(
          new ASM(ASM::MOV, ASM::EQ, {new ASMItem(reg), new ASMItem(0)}));
      break;
    default:
      break;
    }
  } else {
    switch (ir->items[1]->type) {
    case IRItem::FLOAT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A3),
                             new ASMItem(ir->items[1]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A3),
                              new ASMItem(ir->items[1]->iVal / ZERO16)}));
      break;
    case IRItem::FTEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A3),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
      break;
    case IRItem::INT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(ir->items[1]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                              new ASMItem(ir->items[1]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(
          ASM::MOV, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::A1)}));
      break;
    case IRItem::ITEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(
          ASM::MOV, {new ASMItem(ASMItem::A3), new ASMItem(ASMItem::A1)}));
      break;
    default:
      break;
    }
    switch (ir->items[2]->type) {
    case IRItem::FLOAT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A4),
                             new ASMItem(ir->items[2]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A4),
                              new ASMItem(ir->items[2]->iVal / ZERO16)}));
      break;
    case IRItem::FTEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A4),
                             new ASMItem(temp2Reg[ir->items[2]->iVal])}));
      break;
    case IRItem::INT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(ir->items[2]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                              new ASMItem(ir->items[2]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(
          ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::A1)}));
      break;
    case IRItem::ITEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[ir->items[2]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(
          ASM::MOV, {new ASMItem(ASMItem::A4), new ASMItem(ASMItem::A1)}));
      break;
    default:
      break;
    }
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::A3)}));
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(ASMItem::A2), new ASMItem(ASMItem::A4)}));
    switch (ir->type) {
    case IR::EQ:
      asms.push_back(new ASM(ASM::BL, {new ASMItem(" __aeabi_fcmpeq")}));
      break;
    case IR::GE:
      asms.push_back(new ASM(ASM::BL, {new ASMItem(" __aeabi_fcmpge")}));
      break;
    case IR::GT:
      asms.push_back(new ASM(ASM::BL, {new ASMItem(" __aeabi_fcmpgt")}));
      break;
    case IR::LE:
      asms.push_back(new ASM(ASM::BL, {new ASMItem(" __aeabi_fcmple")}));
      break;
    case IR::LT:
      asms.push_back(new ASM(ASM::BL, {new ASMItem(" __aeabi_fcmplt")}));
      break;
    case IR::NE:
      asms.push_back(new ASM(ASM::BL, {new ASMItem(" __aeabi_fcmpun")}));
      break;
    default:
      break;
    }
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
  }
}

void ASMParser::parseDiv(vector<ASM *> &asms, IR *ir) {
  ASMItem::RegType reg = getVReg(ir->items[0]->iVal);
  bool isFloat = false;
  if ((ir->items[1]->type == IRItem::INT ||
       ir->items[1]->type == IRItem::ITEMP) &&
      (ir->items[2]->type == IRItem::INT ||
       ir->items[2]->type == IRItem::ITEMP)) {
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                           new ASMItem(temp2Reg[ir->items[1]->iVal])}));
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(ASMItem::A2),
                           new ASMItem(temp2Reg[ir->items[2]->iVal])}));
    asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_idiv")}));
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
  } else {
    isFloat = true;
    switch (ir->items[1]->type) {
    case IRItem::FLOAT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(ir->items[1]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(reg),
                              new ASMItem(ir->items[1]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::FTEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::INT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(ir->items[1]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                              new ASMItem(ir->items[1]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    case IRItem::ITEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    default:
      break;
    }
    switch (ir->items[2]->type) {
    case IRItem::FLOAT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(ir->items[2]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(reg),
                              new ASMItem(ir->items[2]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::FTEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(temp2Reg[ir->items[2]->iVal])}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::INT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(ir->items[2]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                              new ASMItem(ir->items[2]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    case IRItem::ITEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[ir->items[2]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    default:
      break;
    }
    asms.push_back(new ASM(ASM::POP, {new ASMItem(ASMItem::A2)}));
    asms.push_back(new ASM(ASM::POP, {new ASMItem(ASMItem::A1)}));
    asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_fdiv")}));
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
  }
  if (ir->items[0]->type == IRItem::ITEMP) {
    if (isFloat) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1), new ASMItem(reg)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_f2iz")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
    }
  } else {
    if (!isFloat) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1), new ASMItem(reg)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
    }
  }
}

vector<ASM *> ASMParser::parseFunc(Symbol *symbol, const vector<IR *> &irs) {
  reg2Temp.clear();
  temp2Reg.clear();
  allocReg(irs);
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
  offsets.clear();
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
  if (localVarOffset <= MAX_IMM8 * 3) {
    for (int i = 0; i < localVarOffset / MAX_IMM8; i++)
      asms.push_back(
          new ASM(ASM::SUB, {new ASMItem(ASMItem::SP), new ASMItem(ASMItem::SP),
                             new ASMItem(MAX_IMM8)}));
    asms.push_back(
        new ASM(ASM::SUB, {new ASMItem(ASMItem::SP), new ASMItem(ASMItem::SP),
                           new ASMItem(localVarOffset % MAX_IMM8)}));
  } else {
    ASMItem::RegType reg = popVReg();
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(reg),
                           new ASMItem((unsigned)localVarOffset % ZERO16)}));
    if (localVarOffset > MAX_IMM16)
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(reg),
                              new ASMItem((unsigned)localVarOffset / ZERO16)}));
    asms.push_back(
        new ASM(ASM::SUB, {new ASMItem(ASMItem::SP), new ASMItem(ASMItem::SP),
                           new ASMItem(reg)}));
    pushReg(reg);
  }
  for (unsigned i = 0; i < irs.size(); i++) {
    irId = i;
    bool flag = false;
    asms.push_back(new ASM(ASM::LABEL, {new ASMItem(irLabels[irs[i]])}));
    ASMItem::RegType reg;
    switch (irs[i]->type) {
    case IR::ADD:
      parseAdd(asms, irs[i]);
      break;
    case IR::ARG:
      asms.push_back(
          new ASM(ASM::PUSH, {new ASMItem(temp2Reg[irs[i]->items[0]->iVal])}));
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
      switch (irs[i]->items[0]->symbol ? irs[i]->items[0]->symbol->params.size()
                                       : irs[i]->items[0]->iVal) {
      case 0:
        break;
      case 1:
        asms.push_back(new ASM(ASM::POP, {new ASMItem(ASMItem::A1)}));
        if ((localVarOffset / 4) & 1) {
          flag = true;
          asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::SP)}));
        }
        asms.push_back(new ASM(
            ASM::VMOV, {new ASMItem(ASMItem::S0), new ASMItem(ASMItem::A1)}));
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
      if (flag) {
        asms.push_back(new ASM(
            ASM::STR, {new ASMItem(ASMItem::A1), new ASMItem(ASMItem::SP)}));
        asms.push_back(new ASM(ASM::POP, {new ASMItem(ASMItem::A1)}));
      }
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
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[irs[i]->items[1]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_f2iz")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                             new ASMItem(ASMItem::A1)}));
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
    case IR::I2F:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[irs[i]->items[1]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                             new ASMItem(ASMItem::A1)}));
      break;
    case IR::L_NOT:
      parseLNot(asms, irs[i]);
      break;
    case IR::LOAD:
      reg = getVReg(irs[i]->items[0]->iVal);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(reg),
                             new ASMItem(temp2Reg[irs[i]->items[1]->iVal])}));
      break;
    case IR::MEMSET_ZERO: {
      if (-offsets[irs[i]->items[0]->symbol] > MAX_IMM8) {
        if ((unsigned)(-offsets[irs[i]->items[0]->symbol]) <= MAX_IMM8 * 3) {
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
              ASM::MOV,
              {new ASMItem(ASMItem::A2),
               new ASMItem((unsigned)(-offsets[irs[i]->items[0]->symbol]) %
                           ZERO16)}));
          if (-offsets[irs[i]->items[0]->symbol] > MAX_IMM16)
            asms.push_back(new ASM(
                ASM::MOVT,
                {new ASMItem(ASMItem::A2),
                 new ASMItem((unsigned)(-offsets[irs[i]->items[0]->symbol]) /
                             ZERO16)}));
          asms.push_back(new ASM(ASM::SUB, {new ASMItem(ASMItem::A1),
                                            new ASMItem(ASMItem::FP),
                                            new ASMItem(ASMItem::A2)}));
        }
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
      parseMod(asms, irs[i]);
      break;
    case IR::MOV:
      parseMov(asms, irs[i], irs[i + 1]);
      break;
    case IR::MUL:
      parseMul(asms, irs[i]);
      break;
    case IR::NEG:
      parseNeg(asms, irs[i]);
      break;
    case IR::RETURN:
      if (!irs[i]->items.empty()) {
        asms.push_back(new ASM(
            ASM::MOV,
            {new ASMItem(irs[i]->items[0]->type == IRItem::ITEMP ? ASMItem::A1
                                                                 : ASMItem::A4),
             new ASMItem(temp2Reg[irs[i]->items[0]->iVal])}));
      }
      asms.push_back(new ASM(ASM::B, {new ASMItem(irLabels[irs.back()])}));
      break;
    case IR::STORE:
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(temp2Reg[irs[i]->items[1]->iVal]),
                             new ASMItem(temp2Reg[irs[i]->items[0]->iVal])}));
      break;
    case IR::SUB:
      parseSub(asms, irs[i]);
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

void ASMParser::parseLNot(vector<ASM *> &asms, IR *ir) {
  ASMItem::RegType reg = getVReg(ir->items[0]->iVal);
  if (ir->items[1]->type == IRItem::INT ||
      ir->items[1]->type == IRItem::ITEMP) {
    asms.push_back(new ASM(
        ASM::CMP, {new ASMItem(temp2Reg[ir->items[1]->iVal]), new ASMItem(0)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ, {new ASMItem(reg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::NE, {new ASMItem(reg), new ASMItem(0)}));
  } else {
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                           new ASMItem(temp2Reg[ir->items[1]->iVal])}));
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(ASMItem::A2), new ASMItem(0)}));
    asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_fcmpeq")}));
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
  }
}

void ASMParser::parseMod(vector<ASM *> &asms, IR *ir) {
  ASMItem::RegType reg = getVReg(ir->items[0]->iVal);
  asms.push_back(
      new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                         new ASMItem(temp2Reg[ir->items[1]->iVal])}));
  asms.push_back(
      new ASM(ASM::MOV, {new ASMItem(ASMItem::A2),
                         new ASMItem(temp2Reg[ir->items[2]->iVal])}));
  asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_idivmod")}));
  asms.push_back(
      new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A2)}));
}

void ASMParser::parseMov(vector<ASM *> &asms, IR *ir, IR *nextIr) {
  ASMItem::RegType reg;
  switch (ir->items[1]->type) {
  case IRItem::FLOAT:
    reg = getVReg(ir->items[0]->iVal);
    if (ir->items[0]->type == IRItem::ITEMP) {
      asms.push_back(new ASM(
          ASM::MOV, {new ASMItem(ASMItem::A1),
                     new ASMItem((unsigned)ir->items[1]->iVal % ZERO16)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(ASMItem::A1),
                      new ASMItem((unsigned)ir->items[1]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_f2iz")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
    } else {
      asms.push_back(new ASM(
          ASM::MOV, {new ASMItem(reg),
                     new ASMItem((unsigned)ir->items[1]->iVal % ZERO16)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(reg),
                      new ASMItem((unsigned)ir->items[1]->iVal / ZERO16)}));
    }
    break;
  case IRItem::FTEMP:
    reg = getVReg(ir->items[0]->iVal);
    if (ir->items[0]->type == IRItem::ITEMP) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
      asms.push_back(new ASM(ASM::B, {new ASMItem("__aeabi_f2iz")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
    } else {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
    }
    break;
  case IRItem::INT:
    reg = getVReg(ir->items[0]->iVal);
    if (ir->items[0]->type == IRItem::ITEMP) {
      if (ir->items[1]->iVal < 0 || ir->items[1]->iVal > MAX_IMM16) {
        asms.push_back(new ASM(
            ASM::MOV, {new ASMItem(reg),
                       new ASMItem((unsigned)ir->items[1]->iVal % ZERO16)}));
        asms.push_back(new ASM(
            ASM::MOVT, {new ASMItem(reg),
                        new ASMItem((unsigned)ir->items[1]->iVal / ZERO16)}));
      } else
        asms.push_back(new ASM(
            ASM::MOV, {new ASMItem(reg), new ASMItem(ir->items[1]->iVal)}));
    } else {
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
      asms.push_back(new ASM(ASM::B, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
    }
    break;
  case IRItem::ITEMP:
    reg = getVReg(ir->items[0]->iVal);
    if (ir->items[0]->type == IRItem::ITEMP) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
    } else {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
      asms.push_back(new ASM(ASM::B, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
    }
    break;
  case IRItem::RETURN:
    reg = getVReg(ir->items[0]->iVal);
    asms.push_back(new ASM(
        ASM::MOV,
        {new ASMItem(reg),
         new ASMItem(ir->items[0]->type == IRItem::ITEMP ? ASMItem::A1
                                                         : ASMItem::A4)}));
    break;
  case IRItem::SYMBOL:
    reg = getVReg(ir->items[0]->iVal);
    switch (ir->items[1]->symbol->symbolType) {
    case Symbol::CONST:
    case Symbol::GLOBAL_VAR:
      asms.push_back(new ASM(
          ASM::LDR, {new ASMItem(reg), new ASMItem("l" + to_string(labelId))}));
      asms.push_back(
          new ASM(ASM::LABEL, {new ASMItem("l" + to_string(labelId + 1))}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(reg), new ASMItem(ASMItem::PC),
                             new ASMItem(reg)}));
      asms.push_back(new ASM(ASM::B, {new ASMItem(irLabels[nextIr])}));
      asms.push_back(
          new ASM(ASM::LABEL, {new ASMItem("l" + to_string(labelId++))}));
      asms.push_back(
          new ASM(ASM::TAG, {new ASMItem(ir->items[1]->symbol->name),
                             new ASMItem("l" + to_string(labelId++))}));
      break;
    case Symbol::LOCAL_VAR:
    case Symbol::PARAM:
      if (offsets[ir->items[1]->symbol] >= 0) {
        if (offsets[ir->items[1]->symbol] > MAX_IMM8) {
          if (offsets[ir->items[1]->symbol] <= MAX_IMM8 * 3) {
            asms.push_back(
                new ASM(ASM::ADD, {new ASMItem(reg), new ASMItem(ASMItem::FP),
                                   new ASMItem(MAX_IMM8)}));
            for (int j = 1; j < offsets[ir->items[1]->symbol] / MAX_IMM8; j++)
              asms.push_back(
                  new ASM(ASM::ADD, {new ASMItem(reg), new ASMItem(reg),
                                     new ASMItem(MAX_IMM8)}));
            asms.push_back(new ASM(
                ASM::ADD,
                {new ASMItem(reg), new ASMItem(reg),
                 new ASMItem(offsets[ir->items[1]->symbol] % MAX_IMM8)}));
          } else {
            ASMItem::RegType reg1 = popVReg();
            asms.push_back(new ASM(
                ASM::MOV, {new ASMItem(reg1),
                           new ASMItem((unsigned)offsets[ir->items[1]->symbol] %
                                       ZERO16)}));
            if (offsets[ir->items[1]->symbol] > MAX_IMM16)
              asms.push_back(
                  new ASM(ASM::MOVT,
                          {new ASMItem(reg1),
                           new ASMItem((unsigned)offsets[ir->items[1]->symbol] /
                                       ZERO16)}));
            asms.push_back(
                new ASM(ASM::ADD, {new ASMItem(reg), new ASMItem(ASMItem::FP),
                                   new ASMItem(reg1)}));
            pushReg(reg1);
          }
        } else {
          asms.push_back(
              new ASM(ASM::ADD, {new ASMItem(reg), new ASMItem(ASMItem::FP),
                                 new ASMItem(offsets[ir->items[1]->symbol])}));
        }
      } else {
        if (-offsets[ir->items[1]->symbol] > MAX_IMM8) {
          if (-offsets[ir->items[1]->symbol] <= MAX_IMM8 * 3) {
            asms.push_back(
                new ASM(ASM::SUB, {new ASMItem(reg), new ASMItem(ASMItem::FP),
                                   new ASMItem(MAX_IMM8)}));
            for (int j = 1; j < (-offsets[ir->items[1]->symbol]) / MAX_IMM8;
                 j++)
              asms.push_back(
                  new ASM(ASM::SUB, {new ASMItem(reg), new ASMItem(reg),
                                     new ASMItem(MAX_IMM8)}));
            asms.push_back(new ASM(
                ASM::SUB,
                {new ASMItem(reg), new ASMItem(reg),
                 new ASMItem((-offsets[ir->items[1]->symbol]) % MAX_IMM8)}));
          } else {
            ASMItem::RegType reg1 = popVReg();
            asms.push_back(new ASM(
                ASM::MOV,
                {new ASMItem(reg1),
                 new ASMItem((unsigned)(-offsets[ir->items[1]->symbol]) %
                             ZERO16)}));
            if (-offsets[ir->items[1]->symbol] > MAX_IMM16)
              asms.push_back(new ASM(
                  ASM::MOVT,
                  {new ASMItem(reg1),
                   new ASMItem((unsigned)(-offsets[ir->items[1]->symbol]) /
                               ZERO16)}));
            asms.push_back(
                new ASM(ASM::SUB, {new ASMItem(reg), new ASMItem(ASMItem::FP),
                                   new ASMItem(reg1)}));
            pushReg(reg1);
          }
        } else {
          asms.push_back(
              new ASM(ASM::SUB, {new ASMItem(reg), new ASMItem(ASMItem::FP),
                                 new ASMItem(-offsets[ir->items[1]->symbol])}));
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
  ASMItem::RegType reg = getVReg(ir->items[0]->iVal);
  bool isFloat = false;
  if ((ir->items[1]->type == IRItem::INT ||
       ir->items[1]->type == IRItem::ITEMP) &&
      (ir->items[2]->type == IRItem::INT ||
       ir->items[2]->type == IRItem::ITEMP)) {
    isFloat = false;
    asms.push_back(new ASM(
        ASM::MUL, {new ASMItem(reg), new ASMItem(temp2Reg[ir->items[1]->iVal]),
                   new ASMItem(temp2Reg[ir->items[2]->iVal])}));
  } else {
    isFloat = true;
    switch (ir->items[1]->type) {
    case IRItem::FLOAT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(ir->items[1]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(reg),
                              new ASMItem(ir->items[1]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::FTEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::INT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(ir->items[1]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                              new ASMItem(ir->items[1]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    case IRItem::ITEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    default:
      break;
    }
    switch (ir->items[2]->type) {
    case IRItem::FLOAT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(ir->items[2]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(reg),
                              new ASMItem(ir->items[2]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::FTEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(temp2Reg[ir->items[2]->iVal])}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::INT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(ir->items[2]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                              new ASMItem(ir->items[2]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    case IRItem::ITEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[ir->items[2]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    default:
      break;
    }
    asms.push_back(new ASM(ASM::POP, {new ASMItem(ASMItem::A2)}));
    asms.push_back(new ASM(ASM::POP, {new ASMItem(ASMItem::A1)}));
    asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_fmul")}));
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
  }
  if (ir->items[0]->type == IRItem::ITEMP) {
    if (isFloat) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1), new ASMItem(reg)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_f2iz")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
    }
  } else {
    if (!isFloat) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1), new ASMItem(reg)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
    }
  }
}

void ASMParser::parseNeg(vector<ASM *> &asms, IR *ir) {
  ASMItem::RegType reg = getVReg(ir->items[0]->iVal);
  switch (ir->items[0]->type) {
  case IRItem::FLOAT:
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(reg), new ASMItem(-ir->items[0]->fVal)}));
    break;
  case IRItem::FTEMP:
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(ASMItem::A1), new ASMItem(0)}));
    asms.push_back(
        new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1), new ASMItem(0x8000)}));
    asms.push_back(new ASM(ASM::EOR, {new ASMItem(reg),
                                      new ASMItem(temp2Reg[ir->items[1]->iVal]),
                                      new ASMItem(ASMItem::A1)}));
    break;
  case IRItem::INT:
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(reg), new ASMItem(-ir->items[0]->iVal)}));
    break;
  case IRItem::ITEMP:
    asms.push_back(new ASM(ASM::RSB, {new ASMItem(reg),
                                      new ASMItem(temp2Reg[ir->items[1]->iVal]),
                                      new ASMItem(0)}));
    break;
  default:
    break;
  }
}

void ASMParser::parseSub(vector<ASM *> &asms, IR *ir) {
  ASMItem::RegType reg = getVReg(ir->items[0]->iVal);
  bool isFloat = false;
  if ((ir->items[1]->type == IRItem::INT ||
       ir->items[1]->type == IRItem::ITEMP) &&
      (ir->items[2]->type == IRItem::INT ||
       ir->items[2]->type == IRItem::ITEMP)) {
    isFloat = false;
    asms.push_back(new ASM(
        ASM::SUB, {new ASMItem(reg), new ASMItem(temp2Reg[ir->items[1]->iVal]),
                   new ASMItem(temp2Reg[ir->items[2]->iVal])}));
  } else {
    isFloat = true;
    switch (ir->items[1]->type) {
    case IRItem::FLOAT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(ir->items[1]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(reg),
                              new ASMItem(ir->items[1]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::FTEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::INT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(ir->items[1]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                              new ASMItem(ir->items[1]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    case IRItem::ITEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[ir->items[1]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    default:
      break;
    }
    switch (ir->items[2]->type) {
    case IRItem::FLOAT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(ir->items[2]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(reg),
                              new ASMItem(ir->items[2]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::FTEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg),
                             new ASMItem(temp2Reg[ir->items[2]->iVal])}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(reg)}));
      break;
    case IRItem::INT:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(ir->items[2]->iVal % ZERO16)}));
      asms.push_back(
          new ASM(ASM::MOVT, {new ASMItem(ASMItem::A1),
                              new ASMItem(ir->items[2]->iVal / ZERO16)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    case IRItem::ITEMP:
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1),
                             new ASMItem(temp2Reg[ir->items[2]->iVal])}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(new ASM(ASM::PUSH, {new ASMItem(ASMItem::A1)}));
      break;
    default:
      break;
    }
    asms.push_back(new ASM(ASM::POP, {new ASMItem(ASMItem::A2)}));
    asms.push_back(new ASM(ASM::POP, {new ASMItem(ASMItem::A1)}));
    asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_fsub")}));
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
  }
  if (ir->items[0]->type == IRItem::ITEMP) {
    if (isFloat) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1), new ASMItem(reg)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_f2iz")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
    }
  } else {
    if (!isFloat) {
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(ASMItem::A1), new ASMItem(reg)}));
      asms.push_back(new ASM(ASM::BL, {new ASMItem("__aeabi_i2f")}));
      asms.push_back(
          new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(ASMItem::A1)}));
    }
  }
}

ASMItem::RegType ASMParser::popSReg() {
  const vector<ASMItem::RegType> regs = {
      ASMItem::S15, ASMItem::S14, ASMItem::S13, ASMItem::S12, ASMItem::S11,
      ASMItem::S10, ASMItem::S9,  ASMItem::S8,  ASMItem::S0,  ASMItem::S1,
      ASMItem::S2,  ASMItem::S3,  ASMItem::S4,  ASMItem::S5,  ASMItem::S6,
      ASMItem::S7,  ASMItem::S16, ASMItem::S17, ASMItem::S18, ASMItem::S19,
      ASMItem::S20, ASMItem::S21, ASMItem::S22, ASMItem::S23, ASMItem::S24,
      ASMItem::S25, ASMItem::S26, ASMItem::S27, ASMItem::S28, ASMItem::S29,
      ASMItem::S30, ASMItem::S31};
  for (ASMItem::RegType reg : regs)
    if (reg2Temp.find(reg) == reg2Temp.end()) {
      reg2Temp[reg] = -1;
      return reg;
    }
  cerr << "Regfile not enough!" << endl;
  exit(-1);
  return ASMItem::PC;
}

ASMItem::RegType ASMParser::popVReg() {
  const vector<ASMItem::RegType> regs = {ASMItem::V1, ASMItem::V2, ASMItem::V3,
                                         ASMItem::V4, ASMItem::V5, ASMItem::V6,
                                         ASMItem::V7};
  for (ASMItem::RegType reg : regs)
    if (reg2Temp.find(reg) == reg2Temp.end()) {
      reg2Temp[reg] = -1;
      return reg;
    }
  cerr << "Regfile not enough!" << endl;
  exit(-1);
  return ASMItem::PC;
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

void ASMParser::pushReg(ASMItem::RegType reg) { reg2Temp.erase(reg); }

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