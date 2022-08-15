#ifndef __ASM_OPTIMIZER_H__
#define __ASM_OPTIMIZER_H__

#include <utility>
#include <vector>

#include "../frontend/Symbol.h"
#include "ASM.h"

class ASMOptimizer {
private:
  bool isProcessed;
  std::unordered_map<Symbol *, std::vector<ASM *>> funcASMs;

public:
  ASMOptimizer(const std::unordered_map<Symbol *, std::vector<ASM *>> &);
  ~ASMOptimizer();

  std::unordered_map<Symbol *, std::vector<ASM *>> getFuncASMs();
  void optimize();
  void peepholeOptimize();
};

#endif