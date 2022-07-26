#include <string>
#include <unordered_map>

#include "ASM.h"

using namespace std;

unordered_map<ASM::CondType, string> condTypeStr = {
    {ASM::AL, "al"}, {ASM::CC, "cc"}, {ASM::LO, "lo"}, {ASM::CS, "cs"},
    {ASM::HS, "hs"}, {ASM::EQ, "eq"}, {ASM::GE, "ge"}, {ASM::GT, "gt"},
    {ASM::HI, "hi"}, {ASM::LE, "le"}, {ASM::LS, "ls"}, {ASM::LT, "lt"},
    {ASM::MI, "mi"}, {ASM::NE, "ne"}, {ASM::PL, "pl"}, {ASM::VC, "vc"},
    {ASM::VS, "vs"}};

ASM::ASM(ASMOpType type, const vector<ASMItem *> &items) {
  this->type = type;
  this->cond = AL;
  this->items = items;
}

ASM::ASM(ASMOpType type, CondType cond, const vector<ASMItem *> &items) {
  this->type = type;
  this->cond = cond;
  this->items = items;
}

ASM::~ASM() {
  for (ASMItem *item : items)
    delete item;
}

string ASM::toString() {
  string s;
  bool first = true;
  s += "\t";
  switch (type) {
  case ADD:
    s += "add " + regTypeStr[items[0]->reg] + ", " + regTypeStr[items[1]->reg] +
         ", ";
    switch (items[2]->type) {
    case ASMItem::IMM:
      s += "#" + to_string(items[2]->iVal);
      break;
    case ASMItem::REG:
      s += regTypeStr[items[2]->reg];
      break;
    default:
      break;
    }
    break;
  case B:
    s += "b";
    if (cond != ASM::AL)
      s += condTypeStr[cond];
    s += " l" + to_string(items[0]->iVal);
    break;
  case BL:
    s += "bl " + items[0]->sVal + "(PLT)";
    break;
  case CMP:
    s += "cmp " + regTypeStr[items[0]->reg] + ", ";
    if (items[1]->type == ASMItem::REG)
      s += regTypeStr[items[1]->reg];
    else
      s += "#" + to_string(items[1]->iVal);
    break;
  case EOR:
    s += "eor " + regTypeStr[items[0]->reg] + ", " + regTypeStr[items[1]->reg] +
         ", ";
    switch (items[2]->type) {
    case ASMItem::IMM:
      s += "#" + to_string(items[2]->iVal);
      break;
    case ASMItem::REG:
      s += regTypeStr[items[2]->reg];
      break;
    default:
      break;
    }
    break;
  case LABEL:
    s = "l" + to_string(items[0]->iVal) + ":";
    break;
  case LDR:
    s += "ldr " + regTypeStr[items[0]->reg] + ", ";
    if (items[1]->type != ASMItem::REG) {
      s += "l" + to_string(items[1]->iVal);
      break;
    }
    s += "[" + regTypeStr[items[1]->reg];
    if (items.size() == 3)
      switch (items[2]->type) {
      case ASMItem::IMM:
        s += ", #" + to_string(items[2]->iVal);
        break;
      case ASMItem::REG:
        s += ", " + regTypeStr[items[2]->reg];
        break;
      default:
        break;
      }
    s += "]";
    break;
  case MOV:
    s += "mov";
    if (cond != ASM::AL)
      s += condTypeStr[cond];
    s += " " + regTypeStr[items[0]->reg] + ", ";
    switch (items[1]->type) {
    case ASMItem::IMM:
      s += "#" + to_string(items[1]->iVal);
      break;
    case ASMItem::TAG:
      s += items[1]->sVal;
      break;
    case ASMItem::REG:
      s += regTypeStr[items[1]->reg];
      break;
    default:
      break;
    }
    break;
  case MOVT:
    s += "movt " + regTypeStr[items[0]->reg] + ", ";
    switch (items[1]->type) {
    case ASMItem::IMM:
      s += "#" + to_string(items[1]->iVal);
      break;
    case ASMItem::TAG:
      s += items[1]->sVal;
      break;
    default:
      break;
    }
    break;
  case MOVW:
    s += "movw " + regTypeStr[items[0]->reg] + ", ";
    switch (items[1]->type) {
    case ASMItem::IMM:
      s += "#" + to_string(items[1]->iVal);
      break;
    case ASMItem::TAG:
      s += items[1]->sVal;
      break;
    default:
      break;
    }
    break;
  case MUL:
    s += "mul " + regTypeStr[items[0]->reg] + ", " + regTypeStr[items[1]->reg] +
         ", ";
    switch (items[2]->type) {
    case ASMItem::IMM:
      s += "#" + to_string(items[2]->iVal);
      break;
    case ASMItem::REG:
      s += regTypeStr[items[2]->reg];
      break;
    default:
      break;
    }
    break;
  case POP:
    s += "pop {";
    for (ASMItem *item : items) {
      if (!first)
        s += ", ";
      s += regTypeStr[item->reg];
      first = false;
    }
    s += "}";
    break;
  case PUSH:
    s += "push {";
    for (ASMItem *item : items) {
      if (!first)
        s += ", ";
      s += regTypeStr[item->reg];
      first = false;
    }
    s += "}";
    break;
  case RSB:
    s += "rsb " + regTypeStr[items[0]->reg] + ", " + regTypeStr[items[1]->reg] +
         ", ";
    switch (items[2]->type) {
    case ASMItem::IMM:
      s += "#" + to_string(items[2]->iVal);
      break;
    case ASMItem::REG:
      s += regTypeStr[items[2]->reg];
      break;
    default:
      break;
    }
    break;
  case DIV:
    s += "sdiv " + regTypeStr[items[0]->reg] + ", " +
         regTypeStr[items[1]->reg] + ", " + regTypeStr[items[2]->reg];
    break;
  case STR:
    s += "str " + regTypeStr[items[0]->reg] + ", [" + regTypeStr[items[1]->reg];
    if (items.size() == 3)
      switch (items[2]->type) {
      case ASMItem::IMM:
        s += ", #" + to_string(items[2]->iVal);
        break;
      case ASMItem::REG:
        s += ", " + regTypeStr[items[2]->reg];
        break;
      default:
        break;
      }
    s += "]";
    break;
  case SUB:
    s += "sub " + regTypeStr[items[0]->reg] + ", " + regTypeStr[items[1]->reg] +
         ", ";
    switch (items[2]->type) {
    case ASMItem::IMM:
      s += "#" + to_string(items[2]->iVal);
      break;
    case ASMItem::REG:
      s += regTypeStr[items[2]->reg];
      break;
    default:
      break;
    }
    break;
  case TAG:
    s += ".word " + items[0]->sVal + "-l" + to_string(items[1]->iVal) + "-8";
    break;
  case VADD:
    s += "vadd.f32 " + regTypeStr[items[0]->reg] + ", " +
         regTypeStr[items[1]->reg] + ", " + regTypeStr[items[2]->reg];
    break;
  case VCMP:
    s += "vcmp.f32 " + regTypeStr[items[0]->reg] + ", " +
         regTypeStr[items[1]->reg];
    break;
  case VCVTFS:
    s += "vcvt.s32.f32 " + regTypeStr[items[0]->reg] + ", " +
         regTypeStr[items[1]->reg];
    break;
  case VCVTSF:
    s += "vcvt.f32.s32 " + regTypeStr[items[0]->reg] + ", " +
         regTypeStr[items[1]->reg];
    break;
  case VDIV:
    s += "vdiv.f32 " + regTypeStr[items[0]->reg] + ", " +
         regTypeStr[items[1]->reg] + ", " + regTypeStr[items[2]->reg];
    break;
  case VLDR:
    s += "vldr.f32 " + regTypeStr[items[0]->reg] + ", ";
    if (items[1]->type != ASMItem::REG) {
      s += "l" + to_string(items[1]->iVal);
      break;
    }
    s += "[" + regTypeStr[items[1]->reg];
    if (items.size() == 3)
      switch (items[2]->type) {
      case ASMItem::IMM:
        s += ", #" + to_string(items[2]->iVal);
        break;
      case ASMItem::REG:
        s += ", " + regTypeStr[items[2]->reg];
        break;
      default:
        break;
      }
    s += "]";
    break;
  case VMOV:
    s += "vmov.f32 " + regTypeStr[items[0]->reg] + ", ";
    switch (items[1]->type) {
    case ASMItem::IMM:
      s += "#" + to_string(*(float *)(&items[1]->fVal));
      break;
    case ASMItem::REG:
      s += regTypeStr[items[1]->reg];
      break;
    default:
      break;
    }
    break;
  case VMRS:
    s += "vmrs APSR_nzcv, FPSCR";
    break;
  case VMUL:
    s += "vmul.f32 " + regTypeStr[items[0]->reg] + ", " +
         regTypeStr[items[1]->reg] + ", " + regTypeStr[items[2]->reg];
    break;
  case VNEG:
    s += "vneg.f32 " + regTypeStr[items[0]->reg] + ", " +
         regTypeStr[items[1]->reg];
    break;
  case VPOP:
    s += "vpop.f32 {";
    for (ASMItem *item : items) {
      if (!first)
        s += ", ";
      s += regTypeStr[item->reg];
      first = false;
    }
    s += "}";
    break;
  case VPUSH:
    s += "vpush.f32 {";
    for (ASMItem *item : items) {
      if (!first)
        s += ", ";
      s += regTypeStr[item->reg];
      first = false;
    }
    s += "}";
    break;
  case VSTR:
    s += "vstr.f32 " + regTypeStr[items[0]->reg] + ", [" +
         regTypeStr[items[1]->reg];
    if (items.size() == 3)
      switch (items[2]->type) {
      case ASMItem::IMM:
        s += ", #" + to_string(items[2]->iVal);
        break;
      case ASMItem::REG:
        s += ", " + regTypeStr[items[2]->reg];
        break;
      default:
        break;
      }
    s += "]";
    break;
  case VSUB:
    s += "vsub.f32 " + regTypeStr[items[0]->reg] + ", " +
         regTypeStr[items[1]->reg] + ", " + regTypeStr[items[2]->reg];
    break;
  default:
    return "";
  }
  return s;
}