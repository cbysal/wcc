#include "RegFile.h"

RegFile::RegFile() {
  this->vUsed = 0;
  this->sUsed = 0;
  this->spillUsed = 0;
  this->vRegs = {ASMItem::V8, ASMItem::V7, ASMItem::V6, ASMItem::V5,
                 ASMItem::V4, ASMItem::V3, ASMItem::V2, ASMItem::V1};
  this->sRegs = {ASMItem::S31, ASMItem::S30, ASMItem::S29, ASMItem::S28,
                 ASMItem::S27, ASMItem::S26, ASMItem::S25, ASMItem::S24,
                 ASMItem::S23, ASMItem::S22, ASMItem::S21, ASMItem::S20,
                 ASMItem::S19, ASMItem::S18, ASMItem::S17, ASMItem::S16};
}

RegFile::~RegFile() {}

unsigned RegFile::getUsed(Type type) {
  switch (type) {
  case V:
    return vUsed;
  case S:
    return sUsed;
  case SPILL:
    return spillUsed;
  }
  return 0;
}

pair<ASMItem::RegType, unsigned> RegFile::pop(Type type) {
  ASMItem::RegType reg = ASMItem::SPILL;
  unsigned offset = 0;
  switch (type) {
  case V:
    if (vRegs.empty()) {
      if (spillRegs.empty())
        spillRegs.push_back(spillUsed++);
      offset = spillRegs.back();
      spillRegs.pop_back();
    } else {
      reg = vRegs.back();
      vRegs.pop_back();
      vUsed = max<unsigned>(vUsed, 8 - vRegs.size());
    }
    break;
  case S:
    if (sRegs.empty()) {
      if (spillRegs.empty())
        spillRegs.push_back(spillUsed++);
      offset = spillRegs.back();
      spillRegs.pop_back();
    } else {
      reg = sRegs.back();
      sRegs.pop_back();
      sUsed = max<unsigned>(sUsed, 16 - sRegs.size());
    }
    break;
  default:
    break;
  }
  return {reg, offset};
}

void RegFile::push(ASMItem::RegType reg) {
  if (reg >= ASMItem::A1 && reg <= ASMItem::PC)
    vRegs.push_back(reg);
  else if (reg >= ASMItem::S0 && reg <= ASMItem::S31)
    sRegs.push_back(reg);
}

void RegFile::push(unsigned reg) { spillRegs.push_back(reg); }

vector<ASMItem::RegType> RegFile::getRegs(Type type) {
  if (type == V)
    return vRegs;
  else
    return sRegs;
}

void RegFile::setUsed(Type type, unsigned used) {
  switch (type) {
  case V:
    vUsed = used;
    break;
  case S:
    sUsed = used;
    break;
  case SPILL:
    spillUsed = used;
    break;
  }
}