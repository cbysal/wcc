#ifndef __REG_H__
#define __REG_H__

#include <string>
#include <unordered_map>

namespace Reg {
enum Type {
  SPILL,
  A1,
  A2,
  A3,
  A4,
  V1,
  V2,
  V3,
  V4,
  V5,
  V6,
  V7,
  V8,
  IP,
  SP,
  LR,
  PC,
  S0,
  S1,
  S2,
  S3,
  S4,
  S5,
  S6,
  S7,
  S8,
  S9,
  S10,
  S11,
  S12,
  S13,
  S14,
  S15,
  S16,
  S17,
  S18,
  S19,
  S20,
  S21,
  S22,
  S23,
  S24,
  S25,
  S26,
  S27,
  S28,
  S29,
  S30,
  S31
};
};

extern std::unordered_map<Reg::Type, std::string> regTypeStr;

#endif