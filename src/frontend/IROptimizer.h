#ifndef __IR_OPTIMIZER_H__
#define __IR_OPTIMIZER_H__

#include <unordered_map>
#include <utility>
#include <vector>

#include "IR.h"
#include "Symbol.h"

class IROptimizer {
private:
  void assignPass();
  void constPassBlock();
  void constPassGlobal();
  void deadCodeElimination();
  void optimizeFlow();
  void passInBlock(unsigned, unsigned);
  void peepholeOptimize();
  void singleVar2Reg();
  void splitTemps();
  void standardize(std::vector<IR *> &);

public:
  void optimize();
};

#endif