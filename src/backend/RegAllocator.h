#ifndef __REG_ALLOCATOR_H__
#define __REG_ALLOCATOR_H__

#include <set>
#include <unordered_map>
#include <vector>

#include "../frontend/IR.h"
#include "ASMItem.h"
#include "BasicBlock.h"
#include "RegFile.h"

using namespace std;

class RegAllocator {
private:
  vector<IR *> irs;
  vector<BasicBlock *> bbs;
  bool isProcessed;
  bool coloring;
  RegFile *regs;
  unordered_map<unsigned, ASMItem::RegType> itemp2Reg, ftemp2Reg;
  unordered_map<unsigned, unsigned> temp2SpillReg;
  unordered_map<unsigned, IRItem::IRItemType> tempType;
  unordered_map<unsigned, unordered_set<unsigned>> rMap, wMap;
  unordered_map<unsigned, vector<unsigned>> prevMap;
  unordered_map<unsigned, pair<unsigned, unsigned>> lifespan;
  std::vector<std::set<unsigned>> interfereMatrix;
  std::unordered_set<unsigned> iTemps;
  std::vector<int> useCount;

  void calcLifespan();
  void calcPrevMap();
  void scanSpan(unsigned, unsigned, unordered_set<unsigned> &,
                unordered_set<unsigned> &);
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
  bool testSpill(const vector<vector<unsigned>> &inter, vector<int> degs,
                 int base, int limit, bool update = false,
                 const vector<unsigned> *temps = nullptr);

public:
  RegAllocator(const vector<IR *> &);
  ~RegAllocator();

  void allocate();
  void betterAllocate();
  unordered_map<unsigned, ASMItem::RegType> getFtemp2Reg();
  unordered_map<unsigned, ASMItem::RegType> getItemp2Reg();
  unordered_map<unsigned, unsigned> getTemp2SpillReg();
  vector<unsigned> getUsedRegNum();
};

#endif