#include "ASTOptimizer.h"

using namespace std;

ASTOptimizer::ASTOptimizer(AST *root, const vector<Symbol *> &symbols) {
  this->isProcessed = false;
  this->root = root;
  this->symbols = symbols;
}

ASTOptimizer::~ASTOptimizer() {}

AST *ASTOptimizer::getAST() {
  if (!isProcessed)
    optimize();
  return root;
};

void ASTOptimizer::optimize() { isProcessed = true; };
