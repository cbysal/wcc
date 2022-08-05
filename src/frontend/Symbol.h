#ifndef __SYMBOL_H__
#define __SYMBOL_H__

#include <string>
#include <unordered_map>
#include <vector>

class Symbol {
public:
  enum SymbolType { CONST, FUNC, GLOBAL_VAR, LOCAL_VAR, PARAM };
  enum DataType { FLOAT, INT, VOID };

  SymbolType symbolType;
  DataType dataType;
  std::string name;
  std::vector<Symbol *> params;
  std::vector<int> dimensions;
  union {
    float fVal;
    int iVal;
  };
  std::unordered_map<int, float> fMap;
  std::unordered_map<int, int> iMap;

  Symbol(SymbolType, DataType, const std::string &);
  Symbol(SymbolType, DataType, const std::string &, float);
  Symbol(SymbolType, DataType, const std::string &, int);
  Symbol(SymbolType, DataType, const std::string &,
         const std::vector<Symbol *> &);
  Symbol(SymbolType, DataType, const std::string &, const std::vector<int> &);
  Symbol(SymbolType, DataType, const std::string &, const std::vector<int> &,
         const std::unordered_map<int, float> &);
  Symbol(SymbolType, DataType, const std::string &, const std::vector<int> &,
         const std::unordered_map<int, int> &);
  ~Symbol();

  std::string toString();
};

#endif