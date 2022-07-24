#include <iostream>

#include "AST.h"

using namespace std;

AST::AST(ASTType astType, bool isFloat, OPType opType,
         const vector<AST *> &nodes) {
  this->astType = astType;
  this->isFloat = isFloat;
  this->opType = opType;
  this->dimension = 0;
  this->nodes = nodes;
  this->symbol = nullptr;
}

AST::AST(ASTType astType, bool isFloat, Symbol *symbol,
         const vector<AST *> &nodes) {
  this->astType = astType;
  this->isFloat = isFloat;
  this->dimension = 0;
  this->nodes = nodes;
  this->symbol = symbol;
}

AST::AST(ASTType astType, bool isFloat, const vector<AST *> &nodes) {
  this->astType = astType;
  this->isFloat = isFloat;
  this->dimension = 0;
  this->nodes = nodes;
  this->symbol = nullptr;
}

AST::AST(float fVal) {
  this->astType = FLOAT;
  this->isFloat = true;
  this->fVal = fVal;
  this->dimension = 0;
  this->symbol = nullptr;
}

AST::AST(int iVal) {
  this->astType = INT;
  this->isFloat = false;
  this->iVal = iVal;
  this->dimension = 0;
  this->symbol = nullptr;
}

AST::~AST() {
  for (AST *node : nodes)
    delete node;
}

AST *AST::transIF() {
  if (isFloat) {
    if (astType == FLOAT) {
      isFloat = false;
      astType = INT;
      iVal = fVal;
      return this;
    }
    return new AST(UNARY_EXP, false, F2I, {this});
  } else {
    if (astType == INT) {
      isFloat = true;
      astType = FLOAT;
      fVal = iVal;
      return this;
    }
    return new AST(UNARY_EXP, true, I2F, {this});
  }
}