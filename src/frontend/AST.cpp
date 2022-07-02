#include <iostream>

#include "AST.h"

using namespace std;

AST::AST(ASTType astType) {
  this->symbol = nullptr;
  this->astType = astType;
}

AST::AST(ASTType astType, const vector<AST *> &nodes) {
  this->symbol = nullptr;
  this->astType = astType;
  this->nodes = nodes;
}

AST::AST(ASTType astType, OPType opType) {
  this->symbol = nullptr;
  this->astType = astType;
  this->opType = opType;
}

AST::AST(ASTType astType, OPType opType, const vector<AST *> &nodes) {
  this->symbol = nullptr;
  this->astType = astType;
  this->opType = opType;
  this->nodes = nodes;
}

AST::AST(ASTType astType, Symbol *symbol) {
  this->astType = astType;
  this->symbol = symbol;
}

AST::AST(ASTType astType, Symbol *symbol, const vector<AST *> &nodes) {
  this->astType = astType;
  this->symbol = symbol;
  this->nodes = nodes;
}

AST::AST(float fVal) {
  this->symbol = nullptr;
  this->astType = FLOAT_LITERAL;
  this->fVal = fVal;
}

AST::AST(int iVal) {
  this->symbol = nullptr;
  this->astType = INT_LITERAL;
  this->iVal = iVal;
}

AST::~AST() {
  for (AST *node : nodes)
    delete node;
}