#ifndef __IR_ITEM_H__
#define __IR_ITEM_H__

#include <string>
#include <unordered_map>

#include "IR.h"
#include "Symbol.h"

using namespace std;

class IR;

class IRItem {
public:
  enum IRItemType {
    FLOAT,
    INT,
    IR_OFFSET,
    IR_T,
    PLT,
    RETURN,
    SYMBOL,
    TEMP,
    VAL
  };

  IRItemType type;
  IR *ir;
  Symbol *symbol;
  int iVal;
  float fVal;
  string name;

  IRItem(IRItemType);
  IRItem(IRItemType, IR *);
  IRItem(IRItemType, Symbol *);
  IRItem(IRItemType, float);
  IRItem(IRItemType, int);
  IRItem(IRItemType, string &);
  ~IRItem();
};

extern unordered_map<IRItem::IRItemType, string> irItemTypeStr;

#endif