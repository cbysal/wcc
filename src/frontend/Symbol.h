#ifndef __SYMBOL_H__
#define __SYMBOL_H__

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std;

class Symbol {
public:
  enum SymbolType { CONST, FUNC, GLOBAL_VAR, LOCAL_VAR, PARAM };
  enum DataType { FLOAT, INT, VOID };

  SymbolType symbolType;
  DataType dataType;
  string name;
  vector<Symbol *> params;
  vector<int> dimensions;
  float fVal;
  int iVal;
  unordered_map<int, float> fMap;
  unordered_map<int, int> iMap;

  Symbol(SymbolType, DataType, string &);
  Symbol(SymbolType, DataType, string &, float);
  Symbol(SymbolType, DataType, string &, int);
  Symbol(SymbolType, DataType, string &, vector<Symbol *> &);
  Symbol(SymbolType, DataType, string &, vector<int> &);
  Symbol(SymbolType, DataType, string &, vector<int> &,
         unordered_map<int, float> &);
  Symbol(SymbolType, DataType, string &, vector<int> &,
         unordered_map<int, int> &);
  ~Symbol();

  string toString();
};

#endif