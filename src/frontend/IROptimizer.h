#ifndef __IR_OPTIMIZER_H__
#define __IR_OPTIMIZER_H__

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "IR.h"
#include "Symbol.h"

class IROptimizer {
private:
  void assignPass();
  void constLoopExpand();
  void constPassBlock();
  void constPassGlobal();
  void deadCodeElimination();
  std::unordered_set<Symbol *> getInlinableFuncs();
  void funcInline();
  void global2Local();
  void optimizeFlow();
  void passArr(std::vector<IR *> &);
  void passInBlock(unsigned, unsigned);
  void peepholeOptimize();
  void reassignTempId();
  void singleVar2Reg();
  void splitArrays();
  void splitTemps();
  void standardize(std::vector<IR *> &);

public:
  void optimize();
};

#endif