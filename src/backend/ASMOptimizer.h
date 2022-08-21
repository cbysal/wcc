#ifndef __ASM_OPTIMIZER_H__
#define __ASM_OPTIMIZER_H__

#include "../frontend/Symbol.h"
#include "ASM.h"

class ASMOptimizer {
private:
  void peepholeOptimize();

public:
  void optimize();
};

#endif