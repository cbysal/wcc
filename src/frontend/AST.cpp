#include <iostream>

#include "AST.h"

using namespace std;

AST::AST(ASTType astType, bool isFloat, OPType opType,
         const vector<AST *> &nodes) {
  this->astType = astType;
  this->isFloat = isFloat;
  this->opType = opType;
  this->nodes = nodes;
  this->symbol = nullptr;
}

AST::AST(ASTType astType, bool isFloat, Symbol *symbol,
         const vector<AST *> &nodes) {
  this->astType = astType;
  this->isFloat = isFloat;
  this->symbol = symbol;
  this->nodes = nodes;
}

AST::AST(ASTType astType, bool isFloat, const vector<AST *> &nodes) {
  this->astType = astType;
  this->isFloat = isFloat;
  this->nodes = nodes;
  this->symbol = nullptr;
}

AST::AST(float fVal) {
  this->astType = FLOAT_LITERAL;
  this->isFloat = true;
  this->fVal = fVal;
  this->symbol = nullptr;
}

AST::AST(int iVal) {
  this->astType = INT_LITERAL;
  this->isFloat = false;
  this->iVal = iVal;
  this->symbol = nullptr;
}

AST::~AST() {
  for (AST *node : nodes)
    delete node;
}