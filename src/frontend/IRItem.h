#ifndef __IR_ITEM_H__
#define __IR_ITEM_H__

#include "IR.h"
#include "Symbol.h"
#include "Type.h"

class IR;

class IRItem {
public:
  Type type;
  IR *ir;
  int offset;
  Symbol *symbol;
  int tempId;
  int iVal;
  float fVal;
  string name;

  IRItem(Type);
  IRItem(Type, IR *);
  IRItem(Type, Symbol *);
  IRItem(Type, float);
  IRItem(Type, int);
  IRItem(Type, string);
  IRItem(Type, unsigned long);
  ~IRItem();
};

#endif