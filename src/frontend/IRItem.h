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
    FLOAT,     // float const
    FTEMP,     // float temporary
    INT,       // int const
    ITEMP,     // int temporary
    IR_OFFSET, // unused
    IR_T,      // destination address of jump
    RETURN,
    SYMBOL,
    VAL // unused
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