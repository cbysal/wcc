#include "ASMOptimizer.h"

using namespace std;

ASMOptimizer::ASMOptimizer(
    const vector<pair<Symbol *, vector<ASM *>>> &funcASMs) {
  this->isProcessed = false;
  this->funcASMs = funcASMs;
}

ASMOptimizer::~ASMOptimizer() {}

vector<pair<Symbol *, vector<ASM *>>> ASMOptimizer::getFuncASMs() {
  if (!isProcessed)
    optimize();
  return funcASMs;
}

void ASMOptimizer::optimize() { isProcessed = true; }