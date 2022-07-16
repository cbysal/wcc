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
    FTEMP,
    INT,
    ITEMP,
    IR_OFFSET,
    IR_T,
    RETURN,
    SYMBOL,
    VAL
  };

  IRItemType type;
  IR *ir;
  Symbol *symbol;
  union {
    int iVal;
    float fVal;
  };
  string name;

  IRItem(IR *);
  IRItem(IRItemType);
  IRItem(IRItemType, unsigned);
  IRItem(IRItemType, const string &);
  IRItem(IRItemType, const string &, int);
  IRItem(Symbol *);
  IRItem(float);
  IRItem(int);
  ~IRItem();

  IRItem *clone();
};

extern unordered_map<IRItem::IRItemType, string> irItemTypeStr;

#endif