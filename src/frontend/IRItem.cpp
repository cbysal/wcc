#include "IRItem.h"

using namespace std;

unordered_map<IRItem::IRItemType, string> irItemTypeStr = {
    {IRItem::FLOAT, "FLOAT"},  {IRItem::FTEMP, "FTEMP"},
    {IRItem::INT, "INT"},      {IRItem::IR_T, "IR_T"},
    {IRItem::ITEMP, "ITEMP"},  {IRItem::RETURN, "RETURN"},
    {IRItem::SYMBOL, "SYMBOL"}};

IRItem::IRItem(IR *ir) {
  this->type = IR_T;
  this->ir = ir;
  this->symbol = nullptr;
}

IRItem::IRItem(IRItemType type) {
  this->type = type;
  this->symbol = nullptr;
}

IRItem::IRItem(IRItemType type, unsigned iVal) {
  this->type = type;
  this->iVal = iVal;
  this->symbol = nullptr;
}

IRItem::IRItem(Symbol *symbol) {
  this->type = SYMBOL;
  this->symbol = symbol;
}

IRItem::IRItem(float fVal) {
  this->type = FLOAT;
  this->fVal = fVal;
  this->symbol = nullptr;
}

IRItem::IRItem(int iVal) {
  this->type = INT;
  this->iVal = iVal;
  this->symbol = nullptr;
}

IRItem *IRItem::clone() {
  IRItem *cloneItem = new IRItem(type);
  cloneItem->ir = ir;
  cloneItem->symbol = symbol;
  cloneItem->iVal = iVal;
  return cloneItem;
}

bool IRItem::equals(IRItem *item) {
  if (type != item->type)
    return false;
  switch (type) {
  case FLOAT:
    if (fVal != item->fVal)
      return false;
    break;
  case FTEMP:
  case INT:
  case ITEMP:
    if (iVal != item->iVal)
      return false;
    break;
  case IR_T:
    if (ir != item->ir)
      return false;
    break;
  case SYMBOL:
    if (symbol != item->symbol)
      return false;
    break;
  default:
    break;
  }
  return true;
}