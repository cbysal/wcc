#include "ASMOptimizer.h"

using namespace std;

ASMOptimizer::ASMOptimizer(
    const unordered_map<Symbol *, vector<ASM *>> &funcASMs) {
  this->isProcessed = false;
  this->funcASMs = funcASMs;
}

ASMOptimizer::~ASMOptimizer() {}

unordered_map<Symbol *, vector<ASM *>> ASMOptimizer::getFuncASMs() {
  if (!isProcessed)
    optimize();
  return funcASMs;
}

void ASMOptimizer::optimize() { isProcessed = true; }