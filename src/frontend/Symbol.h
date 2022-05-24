#ifndef __SYMBOL_H__
#define __SYMBOL_H__

#include <string>
#include <utility>
#include <vector>

#include "AST.h"
#include "Type.h"

class Symbol {
public:
  int depth;
  Type symbolType;
  string name;
  Type dataType;
  vector<Symbol *> params;
  vector<int> dimensions;
  float floatValue;
  int intValue;
  float *floatArray;
  int *intArray;

  Symbol();
  ~Symbol();

  string toString();
};

#endif