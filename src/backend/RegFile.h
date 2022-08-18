#ifndef __REG_FILE_H__
#define __REG_FILE_H__

#include <utility>
#include <vector>

#include "ASMItem.h"
#include "Reg.h"

class RegFile {
private:
  unsigned vUsed, sUsed, spillUsed;
  std::vector<Reg::Type> vRegs, sRegs;
  std::vector<unsigned> spillRegs;

public:
  enum Type { V, S, SPILL };

  RegFile();

  unsigned getUsed(Type);
  void setUsed(Type, unsigned);
  std::pair<Reg::Type, unsigned> pop(Type);
  void push(Reg::Type);
  void push(unsigned);
  std::vector<Reg::Type> getRegs(Type type);
};

#endif