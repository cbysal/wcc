#ifndef __ASM_OPTIMIZER_H__
#define __ASM_OPTIMIZER_H__

#include <utility>
#include <vector>

#include "../frontend/Symbol.h"
#include "ASM.h"

class ASMOptimizer {
private:
  bool isProcessed;
  vector<pair<Symbol *, vector<ASM *>>> funcASMs;

public:
  ASMOptimizer(const vector<pair<Symbol *, vector<ASM *>>> &);
  ~ASMOptimizer();

  vector<pair<Symbol *, vector<ASM *>>> getFuncASMs();
  void optimize();
};

#endif