#include "IR.h"

int irIdCount = 0;

IR::IR(Type type) {
  this->irId = irIdCount++;
  this->type = type;
}

IR::IR(Type type, vector<IRItem *> items) {
  this->irId = irIdCount++;
  this->type = type;
  this->items = items;
}

string IR::toString() {
  string s;
  s += '(';
  s += to_string(irId);
  s += ", ";
  s += TYPE_STR.at(type);
  for (IRItem *item : items) {
    s += ", (";
    s += TYPE_STR.at(item->type);
    if (item->type != RETURN)
      s += ", ";
    switch (item->type) {
    case FLOAT:
      s += to_string(item->fVal);
      break;
    case INT:
      s += to_string(item->iVal);
      break;
    case IR_T:
      s += to_string(item->ir->irId);
      break;
    case PLT:
      s += item->name;
      break;
    case SYMBOL:
      s += item->symbol->name;
      break;
    case TEMP:
      s += to_string(item->tempId);
      break;
    default:
      break;
    }
    s += ")";
  }
  s += ')';
  return s;
}

IR::~IR() {}