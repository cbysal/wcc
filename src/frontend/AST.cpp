#include <iostream>

#include "AST.h"

using namespace std;

AST::AST(ASTType astType, DataType dataType, OPType opType,
         const vector<AST *> &nodes) {
  this->astType = astType;
  this->dataType = dataType;
  this->opType = opType;
  this->nodes = nodes;
  this->symbol = nullptr;
}

AST::AST(ASTType astType, DataType dataType, Symbol *symbol,
         const vector<AST *> &nodes) {
  this->astType = astType;
  this->dataType = dataType;
  this->symbol = symbol;
  this->nodes = nodes;
}

AST::AST(ASTType astType, DataType dataType, const vector<AST *> &nodes) {
  this->astType = astType;
  this->dataType = dataType;
  this->nodes = nodes;
  this->symbol = nullptr;
}

AST::AST(float fVal) {
  this->astType = FLOAT_LITERAL;
  this->dataType = FLOAT;
  this->fVal = fVal;
  this->symbol = nullptr;
}

AST::AST(int iVal) {
  this->astType = INT_LITERAL;
  this->dataType = INT;
  this->iVal = iVal;
  this->symbol = nullptr;
}

AST::~AST() {
  for (AST *node : nodes)
    delete node;
}