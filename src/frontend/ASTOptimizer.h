#ifndef __AST_OPTIMIZER_H__
#define __AST_OPTIMIZER_H__

#include <vector>

#include "AST.h"
#include "Symbol.h"

using namespace std;

class ASTOptimizer {
private:
  bool isProcessed;
  AST *root;
  vector<Symbol *> symbols;

public:
  ASTOptimizer(AST *, const vector<Symbol *> &);
  ~ASTOptimizer();

  AST *getAST();
  void optimize();
};

#endif