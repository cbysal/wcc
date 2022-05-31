#ifndef __IR_H__
#define __IR_H__

#include "IRItem.h"
#include "Symbol.h"
#include "Type.h"

class IRItem;

class IR {
public:
  int irId;
  Type type;
  vector<IRItem *> items;

  string toString();

  IR(Type);
  IR(Type, vector<IRItem *>);
  ~IR();
};

#endif