#include <string>
#include <unordered_map>

#include "IR.h"

using namespace std;

int irIdCount = 0;

unordered_map<IR::IRType, string> irTypeStr = {
    {IR::ADD, "ADD"},
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
    {IR::EQ, "EQ"},
    {IR::F2I, "F2I"},
    {IR::GE, "GE"},
    {IR::GOTO, "GOTO"},
    {IR::GT, "GT"},
    {IR::I2F, "I2F"},
    {IR::L_NOT, "L_NOT"},
    {IR::LABEL, "LABEL"},
    {IR::LABEL_WHILE_BEGIN, "LABEL_WHILE_BEGIN"},
    {IR::LABEL_WHILE_END, "LABEL_WHILE_END"},
    {IR::LE, "LE"},
    {IR::LT, "LT"},
    {IR::MEMSET_ZERO, "MEMSET_ZERO"},
    {IR::MOD, "MOD"},
    {IR::MOV, "MOV"},
    {IR::MUL, "MUL"},
    {IR::NE, "NE"},
    {IR::NEG, "NEG"},
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
  string s = to_string(irId) + "\t" + irTypeStr[type];
  for (IRItem *item : items) {
    s += ", ";
    switch (item->type) {
    case IRItem::FLOAT:
      s += to_string(item->fVal);
      break;
    case IRItem::FTEMP:
      s += "f" + to_string(item->iVal);
      break;
    case IRItem::INT:
      s += to_string(item->iVal);
      break;
    case IRItem::ITEMP:
      s += "i" + to_string(item->iVal);
      break;
    case IRItem::IR_T:
      s += to_string(item->ir->irId);
      break;
    case IRItem::RETURN:
      s += "ret";
      break;
    case IRItem::SYMBOL:
      s += item->symbol->name;
      break;
    default:
      break;
    }
  }
  return s;
}