#ifndef __REG_ALLOCATOR_H__
#define __REG_ALLOCATOR_H__

#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../frontend/IR.h"
#include "ASMItem.h"
#include "BasicBlock.h"
#include "Reg.h"
#include "RegFile.h"

class RegAllocator {
private:
  std::vector<IR *> irs;
  std::vector<BasicBlock *> bbs;
  bool isProcessed;
  bool coloring;
  RegFile *regs;
  std::unordered_map<unsigned, Reg::Type> itemp2Reg, ftemp2Reg;
  std::unordered_map<unsigned, unsigned> temp2SpillReg;
  std::unordered_map<unsigned, IRItem::IRItemType> tempType;
  std::unordered_map<unsigned, std::unordered_set<unsigned>> rMap, wMap;
  std::unordered_map<unsigned, std::vector<unsigned>> prevMap;
  std::unordered_map<unsigned, std::pair<unsigned, unsigned>> lifespan;
  std::vector<std::set<unsigned>> interfereMatrix;
  std::unordered_set<unsigned> iTemps;
  std::vector<int> useCount;

  void calcLifespan();
  void calcPrevMap();
  void scanSpan(unsigned, unsigned, std::unordered_set<unsigned> &,
                std::unordered_set<unsigned> &);
  void simpleScan();
  void buildBlocks();
  void calcUseAndDef(unsigned &);
  void dfsTestLoop(std::vector<unsigned> &);
  void topSortBlocks();
  void calcInAndOut();
  void calcInterfere();
  void calcInterfereByLine(int &spillCount);
  void updateInterfereMatrix(
      const std::vector<int> &used,
      const std::vector<std::vector<std::pair<unsigned, unsigned>>> &ranges);
  void updateInterfereMatrix(const std::set<unsigned> &comAlive,
                             const std::set<unsigned> &alive, int &spillCount);
  void logInterfere();
  unsigned graphColoring(IRItem::IRItemType iorf, int &spillCount);
  void constrainSpill(std::vector<unsigned> &spilled, int &spillCount);
  bool testSpill(const std::vector<std::vector<unsigned>> &inter,
                 std::vector<int> degs, int base, int limit,
                 bool update = false,
                 const std::vector<unsigned> *temps = nullptr);

public:
  RegAllocator(const std::vector<IR *> &);
  ~RegAllocator();

  void allocate();
  void betterAllocate();
  std::unordered_map<unsigned, Reg::Type> getFtemp2Reg();
  std::unordered_map<unsigned, Reg::Type> getItemp2Reg();
  std::unordered_map<unsigned, unsigned> getTemp2SpillReg();
  std::vector<unsigned> getUsedRegNum();
};

#endif