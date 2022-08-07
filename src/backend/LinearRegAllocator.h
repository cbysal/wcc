#ifndef __LINEAR_REG_ALLOCATOR_H__
#define __LINEAR_REG_ALLOCATOR_H__

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../frontend/IR.h"
#include "ASMItem.h"
#include "Reg.h"

class LinearRegAllocator {
private:
  const std::vector<Reg::Type> vRegs = {Reg::V1, Reg::V2, Reg::V3, Reg::V4,
                                        Reg::V5, Reg::V6, Reg::V7, Reg::V8};
  const std::vector<Reg::Type> sRegs = {Reg::S16, Reg::S17, Reg::S18, Reg::S19,
                                        Reg::S20, Reg::S21, Reg::S22, Reg::S23,
                                        Reg::S24, Reg::S25, Reg::S26, Reg::S27,
                                        Reg::S28, Reg::S29, Reg::S30, Reg::S31};

  std::vector<IR *> irs;
  bool isProcessed;
  unsigned tempNum;
  unsigned irNum;
  unsigned usedVRegNum;
  unsigned usedSRegNum;
  unsigned spillNum;
  std::unordered_set<unsigned> allocatedTemps;
  std::unordered_map<unsigned, Reg::Type> itemp2Reg, ftemp2Reg;
  std::unordered_map<unsigned, unsigned> temp2SpillReg;
  std::vector<IRItem::IRItemType> tempType;
  std::vector<std::unordered_set<unsigned>> rMap, wMap;
  std::vector<std::vector<unsigned>> prevMap;
  std::vector<std::unordered_set<unsigned>> lifespan;
  std::vector<std::unordered_set<unsigned>> conflictMap;

  void calcConflicts();
  void calcLifespan();
  void calcPrevMap();
  Reg::Type findSReg(unsigned);
  Reg::Type findVReg(unsigned);
  unsigned findSpill(unsigned);
  std::unordered_set<Reg::Type> getConflictSet(unsigned);
  void preProcess();
  void reassignTempId();
  void scanSpan(unsigned, unsigned, std::unordered_set<unsigned> &,
                std::unordered_set<unsigned> &);
  void simpleScan();

public:
  LinearRegAllocator(const std::vector<IR *> &);
  ~LinearRegAllocator();

  void allocate();
  void betterAllocate();
  std::unordered_map<unsigned, Reg::Type> getFtemp2Reg();
  std::vector<IR *> getIRs();
  std::unordered_map<unsigned, Reg::Type> getItemp2Reg();
  std::unordered_map<unsigned, unsigned> getTemp2SpillReg();
  std::vector<unsigned> getUsedRegNum();
};

#endif