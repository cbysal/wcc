#include <algorithm>
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
  calcPrevMap();
  simpleScan();
  calcLifespan();
  vector<unordered_set<unsigned>> begins(irs.size()), ends(irs.size());
  for (pair<unsigned, pair<unsigned, unsigned>> span : lifespan) {
    begins[span.second.first].insert(span.first);
    ends[span.second.second].insert(span.first);
  }
  for (unsigned i = 0; i < irs.size(); i++) {
    for (unsigned temp : ends[i]) {
      if (begins[i].find(temp) == begins[i].end()) {
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
    for (unsigned temp : begins[i]) {
      if (tempType[temp] == IRItem::ITEMP) {
        pair<ASMItem::RegType, unsigned> reg = regs->pop(RegFile::V);
        if (reg.first == ASMItem::SPILL)
          temp2SpillReg[temp] = reg.second;
        else
          itemp2Reg[temp] = reg.first;
        if (ends[i].find(temp) != ends[i].end()) {
          if (reg.first == ASMItem::SPILL)
            regs->push(reg.second);
          else
            regs->push(reg.first);
        }
      }
      if (tempType[temp] == IRItem::FTEMP) {
        pair<ASMItem::RegType, unsigned> reg = regs->pop(RegFile::S);
        if (reg.first == ASMItem::SPILL)
          temp2SpillReg[temp] = reg.second;
        else
          ftemp2Reg[temp] = reg.first;
        if (ends[i].find(temp) != ends[i].end()) {
          if (reg.first == ASMItem::SPILL)
            regs->push(reg.second);
          else
            regs->push(reg.first);
        }
      }
    }
  }
}

void RegAllocator::calcLifespan() {
  simpleScan();
  for (unordered_map<unsigned, pair<unsigned, unsigned>>::iterator it =
           lifespan.begin();
       it != lifespan.end(); it++) {
    for (unsigned rId : rMap[it->first]) {
      unordered_set<unsigned> span, track;
      scanSpan(rId, it->first, span, track);
      if (!span.empty()) {
        it->second.first =
            min(it->second.first, *min_element(span.begin(), span.end()));
        it->second.second =
            max(it->second.second, *max_element(span.begin(), span.end()));
      }
    }
  }
}

void RegAllocator::calcPrevMap() {
  unordered_map<IR *, unsigned> irIdMap;
  for (unsigned i = 0; i < irs.size(); i++)
    irIdMap[irs[i]] = i;
  for (unsigned i = 0; i < irs.size(); i++) {
    if (irs[i]->type == IR::GOTO)
      prevMap[irIdMap[irs[i]->items[0]->ir]].push_back(i);
    else if (irs[i]->type == IR::BEQ || irs[i]->type == IR::BGE ||
             irs[i]->type == IR::BGT || irs[i]->type == IR::BLE ||
             irs[i]->type == IR::BLT || irs[i]->type == IR::BNE) {
      prevMap[i + 1].push_back(i);
      prevMap[irIdMap[irs[i]->items[0]->ir]].push_back(i);
    } else
      prevMap[i + 1].push_back(i);
  }
}

void RegAllocator::scanSpan(unsigned cur, unsigned tId,
                            unordered_set<unsigned> &span,
                            unordered_set<unsigned> &track) {
  if (span.find(cur) != span.end() || track.find(cur) != track.end() ||
      wMap[tId].find(cur) != wMap[tId].end()) {
    span.insert(track.begin(), track.end());
    span.insert(cur);
    return;
  }
  track.insert(cur);
  for (unsigned jId : prevMap[cur])
    scanSpan(jId, tId, span, track);
  track.erase(cur);
}

void RegAllocator::simpleScan() {
  for (unsigned i = 0; i < irs.size(); i++)
    for (unsigned j = 0; j < irs[i]->items.size(); j++)
      if (irs[i]->items[j]->type == IRItem::FTEMP ||
          irs[i]->items[j]->type == IRItem::ITEMP) {
        if (lifespan.find(irs[i]->items[j]->iVal) == lifespan.end()) {
          lifespan[irs[i]->items[j]->iVal] = {i, i};
          tempType[irs[i]->items[j]->iVal] = irs[i]->items[j]->type;
        } else
          lifespan[irs[i]->items[j]->iVal].second = i;
        if (j)
          rMap[irs[i]->items[j]->iVal].insert(i);
        else
          wMap[irs[i]->items[j]->iVal].insert(i);
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
