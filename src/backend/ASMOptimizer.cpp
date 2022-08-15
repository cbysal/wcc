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

void ASMOptimizer::optimize() {
  isProcessed = true;
  peepholeOptimize();
}

void ASMOptimizer::peepholeOptimize() {
  for (unordered_map<Symbol *, vector<ASM *>>::iterator it = funcASMs.begin();
       it != funcASMs.end(); it++) {
    vector<ASM *> &asms = it->second;
    asms.push_back(new ASM(ASM::NOP, {}));
    asms.push_back(new ASM(ASM::NOP, {}));
    asms.push_back(new ASM(ASM::NOP, {}));
    vector<ASM *> newASMs;
    for (unsigned i = 0; i < asms.size() - 3; i++) {
      switch (asms[i]->type) {
      case ASM::B:
        switch (asms[i + 1]->type) {
        case ASM::LABEL:
          if (asms[i]->items[0]->iVal == asms[i + 1]->items[0]->iVal)
            continue;
        default:
          newASMs.push_back(asms[i]);
          break;
        }
        break;
      default:
        newASMs.push_back(asms[i]);
        break;
      }
    }
    while (!newASMs.empty() && newASMs.back()->type == ASM::NOP)
      newASMs.pop_back();
    asms = newASMs;
  }
}