#ifndef __IR_OPTIMIZER_H__
#define __IR_OPTIMIZER_H__

#include <unordered_map>
#include <utility>
#include <vector>

#include "IR.h"
#include "Symbol.h"

class IROptimizer {
private:
  bool isProcessed;
  unsigned tempId;
  std::vector<Symbol *> consts;
  std::vector<Symbol *> globalVars;
  std::unordered_map<Symbol *, std::vector<Symbol *>> localVars;
  std::unordered_map<Symbol *, std::vector<IR *>> funcs;
  std::vector<IR *> toRecycleIRs;

  void constPass();
  void optimize();
  void passInBlock(unsigned, unsigned);
  void removeDeadCode();
  void removeDuplicatedJumps();
  void removeDuplicatedLabels();
  void removeDuplicatedSymbols();
  void singleVar2Reg();
  void splitTemps();
  void splitTempsNotStrict();
  void standardize(std::vector<IR *> &);

public:
  IROptimizer(const std::vector<Symbol *> &, const std::vector<Symbol *> &,
              const std::unordered_map<Symbol *, std::vector<Symbol *>> &,
              const std::unordered_map<Symbol *, std::vector<IR *>> &,
              unsigned);
  ~IROptimizer();

  std::vector<Symbol *> getConsts();
  std::unordered_map<Symbol *, std::vector<IR *>> getFuncs();
  std::vector<Symbol *> getGlobalVars();
  std::unordered_map<Symbol *, std::vector<Symbol *>> getLocalVars();
  unsigned getTempId();
};

#endif