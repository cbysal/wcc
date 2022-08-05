#include <iostream>

#include "AST.h"

using namespace std;

AST::AST(ASTType astType, bool isFloat, Symbol *symbol,
         const vector<AST *> &nodes) {
  this->type = astType;
  this->isFloat = isFloat;
  this->nodes = nodes;
  this->symbol = symbol;
}

AST::AST(ASTType astType, bool isFloat, const vector<AST *> &nodes) {
  this->type = astType;
  this->isFloat = isFloat;
  this->nodes = nodes;
  this->symbol = nullptr;
}

AST::AST(float fVal) {
  this->type = FLOAT;
  this->isFloat = true;
  this->fVal = fVal;
  this->symbol = nullptr;
}

AST::AST(int iVal) {
  this->type = INT;
  this->isFloat = false;
  this->iVal = iVal;
  this->symbol = nullptr;
}

AST::~AST() {
  for (AST *node : nodes)
    delete node;
}

AST *AST::transIF() {
  if (isFloat) {
    if (type == FLOAT) {
      isFloat = false;
      type = INT;
      iVal = fVal;
      return this;
    }
    return new AST(F2I_EXP, false, {this});
  } else {
    if (type == INT) {
      isFloat = true;
      type = FLOAT;
      fVal = iVal;
      return this;
    }
    return new AST(I2F_EXP, true, {this});
  }
}