#include "IROptimizer.h"

IROptimizer::IROptimizer(
    const vector<Symbol *> &symbols, const vector<Symbol *> &consts,
    const vector<Symbol *> &globalVars,
    const unordered_map<Symbol *, vector<Symbol *>> &localVars,
    const vector<pair<Symbol *, vector<IR *>>> &funcs) {
  this->isProcessed = false;
  this->symbols = symbols;
  this->consts = consts;
  this->globalVars = globalVars;
  this->localVars = localVars;
  this->funcs = funcs;
}

IROptimizer::~IROptimizer() {}

vector<pair<Symbol *, vector<IR *>>> IROptimizer::getFuncs() {
  if (!isProcessed)
    optimize();
  return funcs;
}

void IROptimizer::optimize() { isProcessed = true; }