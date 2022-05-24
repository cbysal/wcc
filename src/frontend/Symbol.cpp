#include "Symbol.h"

Symbol::Symbol() {
  this->floatArray = nullptr;
  this->intArray = nullptr;
}

Symbol::~Symbol() {
  if (floatArray) {
    delete[] floatArray;
    floatArray = nullptr;
  }
  if (intValue) {
    delete[] intArray;
    intArray = nullptr;
  }
}

string Symbol::toString() {
  string s;
  bool first = true;
  s += "(";
  s += to_string(depth);
  s += ", ";
  s += TYPE_STR.at(symbolType);
  s += ", ";
  s += name;
  s += ", ";
  switch (symbolType) {
  case CONST:
  case VAR:
    s += TYPE_STR.at(dataType);
    for (int dimension : dimensions)
      s += "[" + to_string(dimension) + "]";
    break;
  case FUNC:
    s += TYPE_STR.at(dataType);
    s += "(";
    for (Symbol *param : params) {
      if (!first)
        s += ", ";
      s += TYPE_STR.at(param->dataType);
      for (int dimension : param->dimensions)
        s += "[" + (dimension == -1 ? "" : to_string(dimension)) + "]";
      first = false;
    }
    s += ")";
    break;
  case PARAM:
    s += TYPE_STR.at(dataType);
    if (!dimensions.empty())
      for (int dimension : dimensions)
        s += "[" + (dimension == -1 ? "" : to_string(dimension)) + "]";
    break;
  default:
    break;
  }
  s += ")";
  return s;
}