#ifndef __SSA_OPTIMIZER_H__
#define __SSA_OPTIMIZER_H__

#include <set>
#include <map>
#include <stack>
#include <string>
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
  std::unordered_map<Symbol *, std::vector<IR *>> funcs;
  std::vector<IR *> toRecycleIRs;

  std::vector<IR *> funIR;      // IR in each function
  std::vector<Symbol *> funVar; // Local variable in each function

  const std::set<IR::IRType> branch = std::set<IR::IRType>{
      IR::BEQ, IR::BGE, IR::BGT, IR::BGT, IR::BLE, IR::BLT, IR::BNE};
  const std::set<IR::IRType> noDef = std::set<IR::IRType>{
      IR::BEQ, IR::BGE,  IR::BGT,  IR::BGT,   IR::BLE,        IR::BLT,
      IR::BNE, IR::CALL, IR::GOTO, IR::LABEL, IR::MEMSET_ZERO};

  using graph = std::vector<std::vector<int>>;

  struct dominatorTree {
    int n, dft;
    graph G, iG, sG;
    std::vector<int> dfn, ord, fa, p, mn, sdom, idom;
    void dfs(int);
    int getFther(int);
    void LengauerTarjan(int);
    void work(graph &, graph &, graph &);
  } domT;

  graph G, T, DF; // Control Flow Gaph, Dominator Tree, Dominator Frontier

  struct SSAvar {
    enum varType {
      LOCAL,
      GLOBAL, // Array is treated as a global variable
      FUNC,
      CONST,
      LEBAL,
      RETURN
    };

    enum dataType {
      FLOAT,
      INT
    };

    union { // Value of constant
      int ival;
      float fval;
    };

    varType type;
    dataType data;
    int tmpID;
    Symbol *symbol;
    SSAvar(IRItem *);
  };

  struct SSAItem {
    SSAvar *var;
    int vid; // variable version
    SSAItem(SSAvar *, int);
  };

  struct SSAIR {
    IR::IRType type;

    // Assignment: L <- op(R)
    // Otherwise: L = NULL
    // Notice: "array[i][j][k] <- x" is treated as "array <- [i][j][k], x"
    SSAItem* L;
    std::vector<SSAItem *> R;

    // phi = 0: L <- op(R) or L = NULL
    // phi = 1: L <- phi(R)
    int phi;

    SSAIR(IR::IRType, SSAItem *, std::vector<SSAItem *>, int);
    std::string toString();
  };

  // Used to convert IRItem to SSAVar
  std::unordered_map<float, SSAvar *> floatCon;
  std::unordered_map<int, SSAvar *> tmpVar, intCon, label;
  std::unordered_map<Symbol *, SSAvar *> symVar;
  SSAvar *RET;
  // Convert IRItem to SSAVar
  SSAvar *getSSAvar(IRItem *);

  // IR of each block
  std::vector<std::vector<IR *>> block;
  // SSAIR of each block after conversion
  std::vector<std::vector<SSAIR *>> ssaIRs;

  // All variables that have occurred
  std::vector<SSAvar *> ssaVars;
  // Block X with definition of V
  std::unordered_map<SSAvar *, std::vector<int> > blockDef;

  // Used variables in each block
  std::vector<std::set<SSAvar *>> usedVar;

  // Stack of variable versions
  std::unordered_map<SSAvar *, std::stack<SSAItem *> > varStack;
  // Version of variable
  std::unordered_map<SSAvar *, int> varVersion;

  void preprocess();
  // Minimal SSA
  void insertPhi();
  void renameVariables(int);
  void translateSSA();
  void clear();
  // Print SSA
  void debug(Symbol *);

  // Optimize
  void optimize();
  int isCopyIR(SSAIR *);
  void copyPropagation();
  void removeUselessPhi();

  std::map<std::vector<SSAItem *>, SSAItem *> csePhiTable;
  std::map<std::tuple<IR::IRType, SSAItem *, SSAItem *>, SSAItem *> cseExpTable;
  std::unordered_map<SSAItem *, SSAItem *> cseCopyTable;
  void commonSubexpressionElimination(int);

  void deadCodeElimination();

public:
  SSAOptimizer(const std::vector<Symbol *> &, const std::vector<Symbol *> &,
               const std::unordered_map<Symbol *, std::vector<Symbol *>> &,
               const std::unordered_map<Symbol *, std::vector<IR *>> &, unsigned);
  ~SSAOptimizer();

  std::vector<Symbol *> getConsts();
  std::unordered_map<Symbol *, std::vector<IR *>> getFuncs();
  std::vector<Symbol *> getGlobalVars();
  std::unordered_map<Symbol *, std::vector<Symbol *>> getLocalVars();
  unsigned getTempId();
  void process();

};

#endif