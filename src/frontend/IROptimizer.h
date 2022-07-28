#ifndef __IR_OPTIMIZER_H__
#define __IR_OPTIMIZER_H__

#include <utility>
#include <vector>

#include "IR.h"
#include "Symbol.h"

class IROptimizer {
private:
  bool isProcessed;
  unsigned tempId;
  vector<Symbol *> consts;
  vector<Symbol *> globalVars;
  unordered_map<Symbol *, vector<Symbol *>> localVars;
  vector<pair<Symbol *, vector<IR *>>> funcs;
  vector<IR *> toRecycleIRs;

  unordered_set<unsigned> getBlockBegins(const vector<IR *> &);
  void optimize();
  void flowOptimize();
  void removeDeadCode();
  void removeDuplicatedJumps();
  void removeDuplicatedLabels();
  void removeDuplicatedSymbols();
  void singleVar2Reg();

public:
  IROptimizer(const vector<Symbol *> &, const vector<Symbol *> &,
              const unordered_map<Symbol *, vector<Symbol *>> &,
              const vector<pair<Symbol *, vector<IR *>>> &, unsigned);
  ~IROptimizer();

  vector<Symbol *> getConsts();
  vector<pair<Symbol *, vector<IR *>>> getFuncs();
  vector<Symbol *> getGlobalVars();
  void printIRs();
};

#endif