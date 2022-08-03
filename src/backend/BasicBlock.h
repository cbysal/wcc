#ifndef _BASIC_BLOCK_H_
#define _BASIC_BLOCK_H_

#include "../frontend/IR.h"
#include <unordered_set>
#include <vector>

struct BasicBlock {
  unsigned bid;
  unsigned first;
  unsigned last;
  bool inLoop = false;
  bool visit = false;
  bool onPath = false;
  std::vector<BasicBlock *> preds;
  std::vector<BasicBlock *> succs;
  std::unordered_set<unsigned> useTemps;
  std::unordered_set<unsigned> defTemps;
  std::unordered_set<unsigned> inTemps;
  std::unordered_set<unsigned> outTemps;

  BasicBlock(unsigned start) : first(start) {}
  void addPred(BasicBlock *p);
  void addSucc(BasicBlock *s);
  void display(std::vector<IR *> &irs);
  bool updateOut();
  bool updateIn();
};

#endif