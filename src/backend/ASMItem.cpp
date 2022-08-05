#include <unordered_map>

#include "ASMItem.h"

using namespace std;

ASMItem::ASMItem(ASMItemType type, int iVal) {
  this->type = type;
  this->iVal = iVal;
}

ASMItem::ASMItem(Reg::Type reg) {
  this->type = REG;
  this->reg = reg;
}

ASMItem::ASMItem(OpType op) {
  this->type = OP;
  this->op = op;
}

ASMItem::ASMItem(float fVal) {
  this->type = FLOAT;
  this->iVal = fVal;
}

ASMItem::ASMItem(int iVal) {
  this->type = INT;
  this->iVal = iVal;
}

ASMItem::ASMItem(unsigned uVal) {
  this->type = INT;
  this->iVal = uVal;
}

ASMItem::ASMItem(const string &sVal) {
  this->type = TAG;
  this->sVal = sVal;
}

ASMItem::~ASMItem() {}
