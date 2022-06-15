#include <string>
#include <unordered_map>

#include "ASMItem.h"

using namespace std;

unordered_map<ASMItem::RegType, string> regTypeStr = {
    {ASMItem::A1, "a1"},   {ASMItem::A2, "a2"},   {ASMItem::A3, "a3"},
    {ASMItem::A4, "a4"},   {ASMItem::V1, "v1"},   {ASMItem::V2, "v2"},
    {ASMItem::V3, "v3"},   {ASMItem::V4, "v4"},   {ASMItem::V5, "v5"},
    {ASMItem::V6, "v6"},   {ASMItem::V7, "v7"},   {ASMItem::FP, "fp"},
    {ASMItem::IP, "ip"},   {ASMItem::SP, "sp"},   {ASMItem::LR, "lr"},
    {ASMItem::PC, "pc"},   {ASMItem::S0, "s0"},   {ASMItem::S1, "s1"},
    {ASMItem::S2, "s2"},   {ASMItem::S3, "s3"},   {ASMItem::S4, "s4"},
    {ASMItem::S5, "s5"},   {ASMItem::S6, "s6"},   {ASMItem::S7, "s7"},
    {ASMItem::S8, "s8"},   {ASMItem::S9, "s9"},   {ASMItem::S10, "s10"},
    {ASMItem::S11, "s11"}, {ASMItem::S12, "s12"}, {ASMItem::S13, "s13"},
    {ASMItem::S14, "s14"}, {ASMItem::S15, "s15"}, {ASMItem::S16, "s16"},
    {ASMItem::S17, "s17"}, {ASMItem::S18, "s18"}, {ASMItem::S19, "s19"},
    {ASMItem::S20, "s20"}, {ASMItem::S21, "s21"}, {ASMItem::S22, "s22"},
    {ASMItem::S23, "s23"}, {ASMItem::S24, "s24"}, {ASMItem::S25, "s25"},
    {ASMItem::S26, "s26"}, {ASMItem::S27, "s27"}, {ASMItem::S28, "s28"},
    {ASMItem::S29, "s29"}, {ASMItem::S30, "s30"}, {ASMItem::S31, "s31"}};

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
