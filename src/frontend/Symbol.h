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
  union {
    float fVal;
    int iVal;
  };
  unordered_map<int, float> fMap;
  unordered_map<int, int> iMap;

  Symbol(SymbolType, DataType, const string &);
  Symbol(SymbolType, DataType, const string &, float);
  Symbol(SymbolType, DataType, const string &, int);
  Symbol(SymbolType, DataType, const string &, const vector<Symbol *> &);
  Symbol(SymbolType, DataType, const string &, const vector<int> &);
  Symbol(SymbolType, DataType, const string &, const vector<int> &,
         const unordered_map<int, float> &);
  Symbol(SymbolType, DataType, const string &, const vector<int> &,
         const unordered_map<int, int> &);
  ~Symbol();

  string toString();
};

#endif