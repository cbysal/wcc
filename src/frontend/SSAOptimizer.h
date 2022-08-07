#ifndef __SSA_OPTIMIZER_H__
#define __SSA_OPTIMIZER_H__

#include <unordered_map>
#include <vector>

#include "IR.h"
#include "Symbol.h"

class SSAOptimizer {
private:
  bool isProcessed;
  unsigned tempId;
  std::vector<Symbol *> consts;
  std::vector<Symbol *> globalVars;
  std::unordered_map<Symbol *, std::vector<Symbol *>> localVars;
  std::vector<std::pair<Symbol *, std::vector<IR *>>> funcs;
  std::vector<IR *> toRecycleIRs;

  void optimize();

public:
  SSAOptimizer(const std::vector<Symbol *> &, const std::vector<Symbol *> &,
               const std::unordered_map<Symbol *, std::vector<Symbol *>> &,
               const std::vector<std::pair<Symbol *, std::vector<IR *>>> &,
               unsigned);
  ~SSAOptimizer();

  std::vector<Symbol *> getConsts();
  std::vector<std::pair<Symbol *, std::vector<IR *>>> getFuncs();
  std::vector<Symbol *> getGlobalVars();
  std::unordered_map<Symbol *, std::vector<Symbol *>> getLocalVars();
  unsigned getTempId();
};

#endif