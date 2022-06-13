#include <string>
#include <unordered_map>

#include "ASMItem.h"

using namespace std;

unordered_map<ASMItem::RegType, string> regTypeStr = {
    {ASMItem::A1, "a1"}, {ASMItem::A2, "a2"}, {ASMItem::A3, "a3"},
    {ASMItem::A4, "a4"}, {ASMItem::V1, "v1"}, {ASMItem::V2, "v2"},
    {ASMItem::V3, "v3"}, {ASMItem::V4, "v4"}, {ASMItem::V5, "v5"},
    {ASMItem::V6, "v6"}, {ASMItem::FP, "fp"}, {ASMItem::IP, "ip"},
    {ASMItem::SP, "sp"}, {ASMItem::LR, "lr"}, {ASMItem::PC, "pc"}};

ASMItem::ASMItem(ASMItemType type, int iVal) {
  this->type = type;
  this->iVal = iVal;
}

ASMItem::ASMItem(RegType reg) {
  this->type = REG;
  this->reg = reg;
}

ASMItem::ASMItem(OpType op) {
  this->type = OP;
  this->op = op;
}

ASMItem::ASMItem(int imm) {
  this->type = IMM;
  this->iVal = imm;
}

ASMItem::ASMItem(string sVal) {
  this->type = LABEL;
  this->sVal = sVal;
}

ASMItem::~ASMItem() {}
