#ifndef __IR_ITEM_H__
#define __IR_ITEM_H__

#include <string>
#include <unordered_map>

#include "IR.h"
#include "Symbol.h"

class IR;

class IRItem {
public:
  enum IRItemType {
    FLOAT,     // float const
    FTEMP,     // float temporary
    INT,       // int const
    ITEMP,     // int temporary
    IR_T,      // destination address of jump
    RETURN,
    SYMBOL
  };

  IRItemType type;
  IR *ir;
  Symbol *symbol;
  union {
    int iVal;
    float fVal;
  };

  IRItem(IR *);
  IRItem(IRItemType);
  IRItem(IRItemType, unsigned);
  IRItem(Symbol *);
  IRItem(float);
  IRItem(int);

  IRItem *clone();
  bool equals(IRItem *);
};

extern std::unordered_map<IRItem::IRItemType, std::string> irItemTypeStr;

#endif