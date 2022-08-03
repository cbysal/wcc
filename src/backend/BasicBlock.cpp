#include "BasicBlock.h"
#include <iostream>

void BasicBlock::addPred(BasicBlock *p) { preds.emplace_back(p); }

void BasicBlock::addSucc(BasicBlock *s) { succs.emplace_back(s); }

void BasicBlock::display(std::vector<IR *> &irs) {
  std::cout << "Basic Block " << bid << "In loop " << inLoop << ": \n";
  for (unsigned i = first; i <= last; i++) {
    std::cout << irs[i]->toString() << '\n';
  }
  std::cout << "Predecessors: \n";
  for (size_t i = 0; i < preds.size(); i++) {
    std::cout << preds[i]->bid << ' ';
  }
  std::cout << "\nSuccessors: \n";
  for (size_t i = 0; i < succs.size(); i++) {
    std::cout << succs[i]->bid << ' ';
  }
  std::cout << "\nUse: \n";
  for (auto i : useTemps) {
    std::cout << i << ' ';
  }
  std::cout << "\nDef: \n";
  for (auto i : defTemps) {
    std::cout << i << ' ';
  }
  std::cout << "\n";
  std::cout << "\nIn: \n";
  for (auto i : inTemps) {
    std::cout << i << ' ';
  }
  std::cout << "\nOut: \n";
  for (auto i : outTemps) {
    std::cout << i << ' ';
  }
  std::cout << "\n";
}

bool BasicBlock::updateOut() {
  unsigned oldSize = outTemps.size();

  for (size_t i = 0; i < succs.size(); i++) {
    for (unsigned t : succs[i]->inTemps) {
      outTemps.insert(t);
    }
  }

  return oldSize == outTemps.size();
}

bool BasicBlock::updateIn() {
  unsigned oldSize = inTemps.size();

  inTemps.clear();
  inTemps.insert(useTemps.begin(), useTemps.end());
  inTemps.insert(outTemps.begin(), outTemps.end());
  for (unsigned t : defTemps) {
    inTemps.erase(t);
  }

  return oldSize == inTemps.size();
}