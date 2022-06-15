#include <string>
#include <unordered_map>

#include "IRItem.h"

using namespace std;

unordered_map<IRItem::IRItemType, string> irItemTypeStr = {
    {IRItem::FLOAT, "FLOAT"},   {IRItem::FTEMP, "FTEMP"},
    {IRItem::INT, "INT"},       {IRItem::IR_OFFSET, "IR_OFFSET"},
    {IRItem::IR_T, "IR_T"},     {IRItem::ITEMP, "ITEMP"},
    {IRItem::PLT, "PLT"},       {IRItem::RETURN, "RETURN"},
    {IRItem::SYMBOL, "SYMBOL"}, {IRItem::VAL, "VAL"}};

IRItem::IRItem(IRItemType type) {
  this->type = type;
  this->symbol = nullptr;
}

IRItem::IRItem(IRItemType type, IR *ir) {
  this->type = type;
  this->ir = ir;
  this->symbol = nullptr;
}

IRItem::IRItem(IRItemType type, Symbol *symbol) {
  this->type = type;
  this->symbol = symbol;
}

IRItem::IRItem(IRItemType type, float val) {
  this->type = type;
  this->fVal = val;
  this->symbol = nullptr;
}

IRItem::IRItem(IRItemType type, int val) {
  this->type = type;
  this->iVal = val;
  this->symbol = nullptr;
}

IRItem::IRItem(IRItemType type, string &val) {
  this->type = type;
  this->name = val;
  this->symbol = nullptr;
}

IRItem::IRItem(IRItemType type, string &sVal, int iVal) {
  this->type = type;
  this->name = sVal;
  this->iVal = iVal;
  this->symbol = nullptr;
}

IRItem::~IRItem() {}
