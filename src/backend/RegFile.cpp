#include "RegFile.h"

using namespace std;

RegFile::RegFile() {
  this->vUsed = 0;
  this->sUsed = 0;
  this->spillUsed = 0;
  this->vRegs = {Reg::V8, Reg::V7, Reg::V6, Reg::V5,
                 Reg::V4, Reg::V3, Reg::V2, Reg::V1};
  this->sRegs = {Reg::S31, Reg::S30, Reg::S29, Reg::S28, Reg::S27, Reg::S26,
                 Reg::S25, Reg::S24, Reg::S23, Reg::S22, Reg::S21, Reg::S20,
                 Reg::S19, Reg::S18, Reg::S17, Reg::S16};
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

pair<Reg::Type, unsigned> RegFile::pop(Type type) {
  Reg::Type reg = Reg::SPILL;
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

void RegFile::push(Reg::Type reg) {
  if (reg >= Reg::A1 && reg <= Reg::PC)
    vRegs.push_back(reg);
  else if (reg >= Reg::S0 && reg <= Reg::S31)
    sRegs.push_back(reg);
}

void RegFile::push(unsigned reg) { spillRegs.push_back(reg); }

vector<Reg::Type> RegFile::getRegs(Type type) {
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