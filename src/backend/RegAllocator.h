#ifndef __REG_ALLOCATOR_H__
#define __REG_ALLOCATOR_H__

#include <unordered_map>
#include <vector>

#include "../frontend/IR.h"
#include "ASMItem.h"
#include "RegFile.h"

using namespace std;

class RegAllocator {
private:
  vector<IR *> irs;
  bool isProcessed;
  RegFile *regs;
  unordered_map<unsigned, ASMItem::RegType> itemp2Reg, ftemp2Reg;
  unordered_map<unsigned, unsigned> temp2SpillReg;

public:
  RegAllocator(const vector<IR *> &);
  ~RegAllocator();

  void allocate();
  unordered_map<unsigned, ASMItem::RegType> getFtemp2Reg();
  unordered_map<unsigned, ASMItem::RegType> getItemp2Reg();
  unordered_map<unsigned, unsigned> getTemp2SpillReg();
  vector<unsigned> getUsedRegNum();
};

#endif