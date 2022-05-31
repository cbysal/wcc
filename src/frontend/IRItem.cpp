#include "IRItem.h"

IRItem::IRItem(Type type) { this->type = type; }

IRItem::IRItem(Type type, IR *ir) {
  this->type = type;
  this->ir = ir;
}

IRItem::IRItem(Type type, Symbol *symbol) {
  this->type = type;
  this->symbol = symbol;
}

IRItem::IRItem(Type type, float val) {
  this->type = type;
  this->fVal = val;
}

IRItem::IRItem(Type type, int val) {
  this->type = type;
  switch (type) {
  case INT:
    this->iVal = val;
    break;
  case IR_OFFSET:
    this->offset = val;
    break;
  case TEMP:
    this->tempId = val;
    break;
  default:
    break;
  }
}

IRItem::IRItem(Type type, string val) {
  this->type = type;
  this->name = val;
}

IRItem::IRItem(Type type, unsigned long val) {
  this->type = type;
  switch (type) {
  case INT:
    this->iVal = val;
    break;
  case IR_OFFSET:
    this->offset = val;
    break;
  case TEMP:
    this->tempId = val;
    break;
  default:
    break;
  }
}

IRItem::~IRItem() {}
