#include <unordered_set>

#include "RegAllocator.h"

using namespace std;

RegAllocator::RegAllocator(const vector<IR *> &irs) {
  this->irs = irs;
  this->isProcessed = false;
  this->regs = new RegFile();
}

RegAllocator::~RegAllocator() { delete regs; }

void RegAllocator::allocate() {
  isProcessed = true;
  unordered_map<unsigned, pair<unsigned, unsigned>> lifespan;
  unordered_map<IR *, unsigned> irMap;
  unordered_map<unsigned, IRItem::IRItemType> tempType;
  vector<pair<unsigned, unsigned>> intervals;
  for (unsigned i = 0; i < irs.size(); i++)
    irMap[irs[i]] = i;
  for (unsigned i = 0; i < irs.size(); i++)
    if ((irs[i]->type == IR::BEQ || irs[i]->type == IR::BGE ||
         irs[i]->type == IR::BGT || irs[i]->type == IR::BLE ||
         irs[i]->type == IR::BLT || irs[i]->type == IR::BNE ||
         irs[i]->type == IR::GOTO) &&
        i > irMap[irs[i]->items[0]->ir])
      intervals.emplace_back(irMap[irs[i]->items[0]->ir], i);
  for (unsigned i = 0; i < irs.size(); i++)
    for (IRItem *item : irs[i]->items)
      if (item->type == IRItem::FTEMP || item->type == IRItem::ITEMP) {
        if (lifespan.find(item->iVal) == lifespan.end()) {
          lifespan[item->iVal] = {i, i};
          tempType[item->iVal] = item->type;
        } else
          lifespan[item->iVal].second = i;
      }
  for (unordered_map<unsigned, pair<unsigned, unsigned>>::iterator it =
           lifespan.begin();
       it != lifespan.end(); it++) {
    bool flag = true;
    while (flag) {
      flag = false;
      for (pair<unsigned, unsigned> interval : intervals) {
        if (interval.first < it->second.first &&
            it->second.first <= interval.second &&
            interval.second <= it->second.second) {
          flag = true;
          it->second.first = interval.first;
        }
        if (it->second.first <= interval.first &&
            interval.first <= it->second.second &&
            it->second.second < interval.second) {
          flag = true;
          it->second.second = interval.second;
        }
      }
    }
  }
  unordered_map<unsigned, unordered_set<unsigned>> begins, ends;
  for (pair<unsigned, pair<unsigned, unsigned>> span : lifespan) {
    begins[span.second.first].insert(span.first);
    ends[span.second.second].insert(span.first);
  }
  for (unsigned i = 0; i < irs.size(); i++) {
    if (ends.find(i) != ends.end()) {
      for (unsigned temp : ends[i]) {
        if (tempType[temp] == IRItem::ITEMP) {
          if (itemp2Reg.find(temp) == itemp2Reg.end())
            regs->push(temp2SpillReg[temp]);
          else
            regs->push(itemp2Reg[temp]);
        }
        if (tempType[temp] == IRItem::FTEMP) {
          if (ftemp2Reg.find(temp) == ftemp2Reg.end())
            regs->push(temp2SpillReg[temp]);
          else
            regs->push(ftemp2Reg[temp]);
        }
      }
    }
    if (begins.find(i) != begins.end()) {
      for (unsigned temp : begins[i]) {
        if (tempType[temp] == IRItem::ITEMP) {
          pair<ASMItem::RegType, unsigned> reg = regs->pop(RegFile::V);
          if (reg.first == ASMItem::SPILL)
            temp2SpillReg[temp] = reg.second;
          else
            itemp2Reg[temp] = reg.first;
        }
        if (tempType[temp] == IRItem::FTEMP) {
          pair<ASMItem::RegType, unsigned> reg = regs->pop(RegFile::S);
          if (reg.first == ASMItem::SPILL)
            temp2SpillReg[temp] = reg.second;
          else
            ftemp2Reg[temp] = reg.first;
        }
      }
    }
  }
}

unordered_map<unsigned, ASMItem::RegType> RegAllocator::getFtemp2Reg() {
  if (!isProcessed)
    allocate();
  return ftemp2Reg;
}

unordered_map<unsigned, ASMItem::RegType> RegAllocator::getItemp2Reg() {
  if (!isProcessed)
    allocate();
  return itemp2Reg;
}

unordered_map<unsigned, unsigned> RegAllocator::getTemp2SpillReg() {
  if (!isProcessed)
    allocate();
  return temp2SpillReg;
}

vector<unsigned> RegAllocator::getUsedRegNum() {
  if (!isProcessed)
    allocate();
  return {regs->getUsed(RegFile::V), regs->getUsed(RegFile::S),
          regs->getUsed(RegFile::SPILL)};
}
