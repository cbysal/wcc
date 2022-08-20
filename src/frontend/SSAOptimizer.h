#ifndef __SSA_OPTIMIZER_H__
#define __SSA_OPTIMIZER_H__

#include <map>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "IR.h"
#include "Symbol.h"

class SSAOptimizer {
private:
  Symbol *funSym;
  std::vector<IR *> funIR; // IR in each function

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
    void work(graph &, graph &, graph &, graph &);
  } domT;

  // Control Flow Gaph, Inverse graph of CGF,
  graph G, iG;
  // Dominator Tree, Dominator Frontier
  graph T, DF;

  struct IRvar {
    enum varType {
      LOCAL,
      GLOBAL, // Array is treated as a global variable
      FUNC,
      CONST,
      LEBAL,
      RETURN
    };

    enum dataType { FLOAT, INT };

    union { // Value of constant
      int ival;
      float fval;
    };

    varType type;
    dataType data;
    int tmpID;
    Symbol *symbol;
    IRvar(IRItem *);
    IRvar(int);
    IRvar(float);
    IRvar();
  };

  struct SSAvar {
    IRvar *var;
    int vid; // variable version
    SSAvar(IRvar *, int);
    SSAvar(SSAvar *);
  };
  
  struct SSAItem {
    SSAvar *x;
    int def; // defined in which block
    SSAItem(SSAvar *, int);
    SSAItem(IRvar *, int, int);

    SSAvar *operator->() { return x; }
    bool operator<(SSAItem a) const { return x < a.x; }
  };

  struct SSAIR {
    IR::IRType type;

    // Assignment: L <- op(R)
    // Otherwise: L = NULL
    // Notice: "array[i][j][k] <- x" is treated as "array <- [i][j][k], x"
    SSAItem L;
    std::vector<SSAItem> R;

    // phi = 0: L <- op(R) or L = NULL
    // phi = 1: L <- phi(R)
    int phi;

    SSAIR(IR::IRType, SSAItem, std::vector<SSAItem>, int);
    std::string toString();

    std::vector<SSAvar *> to_SSAvarR();
  };

  // Used to convert IRItem to SSAVar
  std::unordered_map<float, IRvar *> floatCon;
  std::unordered_map<int, IRvar *> tmpVar, intCon, label;
  std::unordered_map<Symbol *, IRvar *> symVar;
  IRvar *RET;
  // Convert IRItem to SSAVar
  IRvar *getSSAvar(IRItem *);

  // IR of each block
  std::vector<std::vector<IR *>> block;
  // SSAIR of each block after conversion
  std::vector<std::vector<SSAIR *>> ssaIRs;
  // SSAIR added after each block during conversion of SSA to normal form
  std::vector<std::vector<SSAIR *>> insIRs;

  // All variables that have occurred
  std::vector<IRvar *> ssaVars;
  // Block X with definition of V
  std::unordered_map<IRvar *, std::vector<int>> blockDef;

  // Used variables in each block
  std::vector<std::set<IRvar *>> usedVar;

  // Stack of variable versions
  std::unordered_map<IRvar *, std::stack<SSAItem>> varStack;
  // Version of variable
  std::unordered_map<IRvar *, int> varVersion;

  // main function
  void ssaConstruction();
  void ssaDestruction();
  void optimize();

  // SSA Constrution
  void preprocess();
  void insertPhi(); // Minimal SSA
  void renameVariables(int);

  // Optimize
  void copyPropagation();
  void constantPropagation();
  void removeUselessPhi();
  std::map<std::vector<SSAvar *>, SSAItem> csePhiTable;
  std::map<std::tuple<IR::IRType, SSAvar *, SSAvar *>, SSAItem> cseExpTable;
  std::unordered_map<SSAvar *, SSAItem> cseCopyTable;
  void commonSubexpressionElimination(int);
  void moveInvariantExpOut();
  void deadCodeElimination();

  // Utils
  void printSSA();
  void clear();
  int isCopyIR(SSAIR *);
  SSAIR *newCopyIR(SSAItem, int);

public:
  std::vector<Symbol *> getConsts();
  std::unordered_map<Symbol *, std::vector<IR *>> getFuncs();
  std::vector<Symbol *> getGlobalVars();
  std::unordered_map<Symbol *, std::vector<Symbol *>> getLocalVars();
  unsigned getTempId();
  void process();
};

#endif