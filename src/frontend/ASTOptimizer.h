#ifndef __AST_OPTIMIZER_H__
#define __AST_OPTIMIZER_H__

#include <vector>

#include "AST.h"
#include "Symbol.h"

class AST;
class Symbol;

class ASTOptimizer {
private:
public:
  void optimize();
};

#endif