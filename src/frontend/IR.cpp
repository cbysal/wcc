#include <string>
#include <unordered_map>

#include "IR.h"

using namespace std;

int irIdCount = 0;

unordered_map<IR::IRType, string> irTypeStr = {
    {IR::ADD, "ADD"},
    {IR::ARG, "ARG"},
    {IR::BEQ, "BEQ"},
    {IR::BGE, "BGE"},
    {IR::BGT, "BGT"},
    {IR::BLE, "BLE"},
    {IR::BLT, "BLT"},
    {IR::BNE, "BNE"},
    {IR::BREAK, "BREAK"},
    {IR::CALL, "CALL"},
    {IR::CONTINUE, "CONTINUE"},
    {IR::DIV, "DIV"},
    {IR::FUNC_END, "FUNC_END"},
    {IR::GOTO, "GOTO"},
    {IR::LABEL_WHILE_BEGIN, "LABEL_WHILE_BEGIN"},
    {IR::LABEL_WHILE_END, "LABEL_WHILE_END"},
    {IR::LOAD, "LOAD"},
    {IR::MEMSET_ZERO, "MEMSET_ZERO"},
    {IR::MOD, "MOD"},
    {IR::MOV, "MOV"},
    {IR::MUL, "MUL"},
    {IR::NEG, "NEG"},
    {IR::PLT, "PLT"},
    {IR::POP, "POP"},
    {IR::POS, "POS"},
    {IR::PUSH, "PUSH"},
    {IR::RETURN, "RETURN"},
    {IR::STORE, "STORE"},
    {IR::SUB, "SUB"}};

IR::IR(IRType type) {
  this->irId = irIdCount++;
  this->type = type;
}

IR::IR(IRType type, const vector<IRItem *> &items) {
  this->irId = irIdCount++;
  this->type = type;
  this->items = items;
}

IR::~IR() {
  for (IRItem *item : items)
    delete item;
}

string IR::toString() {
  string s = "(" + to_string(irId) + ", " + irTypeStr[type];
  for (IRItem *item : items) {
    s += ", (" + irItemTypeStr[item->type];
    if (item->type != IRItem::RETURN)
      s += ", ";
    switch (item->type) {
    case IRItem::FLOAT:
      s += to_string(item->fVal);
      break;
    case IRItem::INT:
      s += to_string(item->iVal);
      break;
    case IRItem::IR_T:
      s += to_string(item->ir->irId);
      break;
    case IRItem::PLT:
      s += item->name;
      break;
    case IRItem::SYMBOL:
      s += item->symbol->name;
      break;
    case IRItem::TEMP:
      s += to_string(item->iVal);
      break;
    default:
      break;
    }
    s += ")";
  }
  s += ")";
  return s;
}