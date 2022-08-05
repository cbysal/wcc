#ifndef __AST_OPTIMIZER_H__
#define __AST_OPTIMIZER_H__

#include <vector>

#include "AST.h"
#include "Symbol.h"

class AST;
class Symbol;

class ASTOptimizer {
private:
  bool isProcessed;
  AST *root;
  std::vector<Symbol *> symbols;

public:
  ASTOptimizer(AST *, const std::vector<Symbol *> &);
  ~ASTOptimizer();

  AST *getAST();
  void optimize();
};

#endif