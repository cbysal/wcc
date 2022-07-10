#ifndef __IR_OPTIMIZER_H__
#define __IR_OPTIMIZER_H__

#include <utility>
#include <vector>

#include "IR.h"
#include "Symbol.h"

class IROptimizer {
private:
  bool isProcessed;
  vector<Symbol *> symbols;
  vector<Symbol *> consts;
  vector<Symbol *> globalVars;
  unordered_map<Symbol *, vector<Symbol *>> localVars;
  vector<pair<Symbol *, vector<IR *>>> funcs;

public:
  IROptimizer(const vector<Symbol *> &, const vector<Symbol *> &,
              const vector<Symbol *> &,
              const unordered_map<Symbol *, vector<Symbol *>> &,
              const vector<pair<Symbol *, vector<IR *>>> &);
  ~IROptimizer();

  void deleteDeadCode();
  vector<pair<Symbol *, vector<IR *>>> getFuncs();
  void optimize();
};

#endif