#ifndef __ASM_OPTIMIZER_H__
#define __ASM_OPTIMIZER_H__

#include <set>
#include <utility>
#include <vector>

#include "../frontend/Symbol.h"
#include "ASM.h"
#include "BasicBlock.h"

class ASMOptimizer {
private:
  void peepholeOptimize();
  void scheduleOptimize();
  std::vector<AsmBasicBlock> buildBlock(std::vector<ASM *>);

public:
  void optimize();
};

#endif