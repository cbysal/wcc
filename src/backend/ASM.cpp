#include <string>
#include <unordered_map>

#include "ASM.h"

using namespace std;

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
    switch (cond) {
    case EQ:
      s += "eq";
      break;
    case GE:
      s += "ge";
      break;
    case GT:
      s += "gt";
      break;
    case LE:
      s += "le";
      break;
    case LT:
      s += "lt";
      break;
    case NE:
      s += "ne";
      break;
    default:
      break;
    }
    s += " " + items[0]->sVal;
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
    s = items[0]->sVal + ":";
    break;
  case LDR:
    s += "ldr " + regTypeStr[items[0]->reg] + ", ";
    if (items[1]->type == ASMItem::LABEL) {
      s += items[1]->sVal;
      break;
    }
    s += "[" + regTypeStr[items[1]->reg];
    if (items.size() == 3)
      s += ", #" + to_string(items[2]->iVal);
    s += "]";
    break;
  case MOV:
    s += "mov";
    switch (cond) {
    case EQ:
      s += "eq";
      break;
    case GE:
      s += "ge";
      break;
    case GT:
      s += "gt";
      break;
    case LE:
      s += "le";
      break;
    case LT:
      s += "lt";
      break;
    case NE:
      s += "ne";
      break;
    default:
      break;
    }
    s += " " + regTypeStr[items[0]->reg] + ", ";
    switch (items[1]->type) {
    case ASMItem::IMM:
      s += "#" + to_string(items[1]->iVal);
      break;
    case ASMItem::REG:
      s += regTypeStr[items[1]->reg];
      break;
    default:
      break;
    }
    break;
  case MOVT:
    s +=
        "movt " + regTypeStr[items[0]->reg] + ", #" + to_string(items[1]->iVal);
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
  case STR:
    s += "str " + regTypeStr[items[0]->reg] + ", [" + regTypeStr[items[1]->reg];
    if (items.size() == 3)
      s += ", #" + to_string(items[1]->iVal);
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
    s += ".word " + items[0]->sVal + "-" + items[1]->sVal + "-8";
    break;
  case VLDR:
    s += "vldr.32 " + regTypeStr[items[0]->reg] + ", ";
    if (items[1]->type == ASMItem::LABEL) {
      s += items[1]->sVal;
      break;
    }
    s += "[" + regTypeStr[items[1]->reg];
    if (items.size() == 3)
      s += ", #" + to_string(items[2]->iVal);
    s += "]";
    break;
  case VMOV:
    s += "vmov.f32 " + regTypeStr[items[0]->reg] + ", " +
         regTypeStr[items[1]->reg];
    break;
  default:
    return "";
  }
  return s;
}