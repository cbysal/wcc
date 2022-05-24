#ifndef __AST_H__
#define __AST_H__

#include <string>
#include <vector>

#include "Symbol.h"
#include "Type.h"

using namespace std;

class Symbol;

class AST {
public:
  Type astType;
  Type type;
  string name;
  Symbol *symbol;
  int iVal;
  float fVal;
  vector<AST *> nodes;
  vector<Type> ops;

  AST(Type);
  AST(Type, AST *);
  AST(Type, AST *, AST *);
  AST(Type, AST *, AST *, AST *);
  AST(Type, Symbol *);
  AST(Type, Symbol *, AST *);
  AST(Type, Symbol *, vector<AST *>);
  AST(Type, Type);
  AST(Type, Type, AST *);
  AST(Type, Type, AST *, AST *);
  AST(Type, Type, string);
  AST(Type, Type, string, vector<AST *>);
  AST(Type, Type, string, vector<AST *>, AST *);
  AST(Type, Type, vector<AST *>);
  AST(Type, string, vector<AST *>);
  AST(Type, vector<AST *>);
  AST(Type, vector<AST *>, vector<Type>);
  AST(float);
  AST(int);
  ~AST();

  void print(int);
};

#endif