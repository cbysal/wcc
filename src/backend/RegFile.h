#ifndef __REG_FILE_H__
#define __REG_FILE_H__

#include <utility>
#include <vector>

#include "ASMItem.h"

using namespace std;

class RegFile {
private:
  unsigned vUsed, sUsed, spillUsed;
  vector<ASMItem::RegType> vRegs, sRegs;
  vector<unsigned> spillRegs;

public:
  enum Type { V, S, SPILL };

  RegFile();
  ~RegFile();

  unsigned getUsed(Type);
  pair<ASMItem::RegType, unsigned> pop(Type);
  void push(ASMItem::RegType);
  void push(unsigned);
};

#endif