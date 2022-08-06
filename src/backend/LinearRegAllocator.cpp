#include <algorithm>

#include "LinearRegAllocator.h"

using namespace std;

LinearRegAllocator::LinearRegAllocator(const vector<IR *> &irs) {
  this->irs = irs;
  this->isProcessed = false;
  this->usedVRegNum = 0;
  this->usedSRegNum = 0;
  this->spillNum = 0;
}

LinearRegAllocator::~LinearRegAllocator() {}

void LinearRegAllocator::allocate() {
  isProcessed = true;
  preProcess();
  calcPrevMap();
  simpleScan();
  calcLifespan();
  calcConflicts();
  for (unsigned i = 0; i < irs.size(); i++) {
    for (IRItem *item : irs[i]->items) {
      if (item->type != IRItem::FTEMP && item->type != IRItem::ITEMP)
        continue;
      if (allocatedTemps.find(item->iVal) != allocatedTemps.end())
        continue;
      allocatedTemps.insert(item->iVal);
      if (item->type == IRItem::FTEMP) {
        Reg::Type reg = findSReg(item->iVal);
        if (reg == Reg::SPILL) {
          unsigned toSpillTemp = item->iVal;
          for (unsigned conflictTempId : conflictMap[item->iVal]) {
            if (ftemp2Reg.find(conflictTempId) != ftemp2Reg.end()) {
              if (conflictMap[conflictTempId].size() >
                  conflictMap[toSpillTemp].size()) {
                toSpillTemp = conflictTempId;
              }
            }
          }
          if (toSpillTemp != (unsigned)item->iVal) {
            ftemp2Reg[item->iVal] = ftemp2Reg[toSpillTemp];
            ftemp2Reg.erase(toSpillTemp);
          }
        } else {
          ftemp2Reg[item->iVal] = reg;
        }
      }
      if (item->type == IRItem::ITEMP) {
        Reg::Type reg = findVReg(item->iVal);
        if (reg == Reg::SPILL) {
          unsigned toSpillTemp = item->iVal;
          for (unsigned conflictTempId : conflictMap[item->iVal]) {
            if (itemp2Reg.find(conflictTempId) != itemp2Reg.end()) {
              if (conflictMap[conflictTempId].size() >
                  conflictMap[toSpillTemp].size()) {
                toSpillTemp = conflictTempId;
              }
            }
          }
          if (toSpillTemp != (unsigned)item->iVal) {
            itemp2Reg[item->iVal] = itemp2Reg[toSpillTemp];
            itemp2Reg.erase(toSpillTemp);
          }
        } else {
          itemp2Reg[item->iVal] = reg;
        }
      }
    }
  }
  allocatedTemps.clear();
  for (unsigned i = 0; i < irs.size(); i++) {
    for (IRItem *item : irs[i]->items) {
      if (item->type != IRItem::FTEMP && item->type != IRItem::ITEMP)
        continue;
      if (allocatedTemps.find(item->iVal) != allocatedTemps.end())
        continue;
      if (item->type == IRItem::FTEMP &&
          ftemp2Reg.find(item->iVal) != ftemp2Reg.end())
        continue;
      if (item->type == IRItem::ITEMP &&
          itemp2Reg.find(item->iVal) != itemp2Reg.end())
        continue;
      allocatedTemps.insert(item->iVal);
      temp2SpillReg[item->iVal] = findSpill(item->iVal);
    }
  }
}

void LinearRegAllocator::calcConflicts() {
  unordered_map<unsigned, vector<pair<unsigned, unsigned>>> intervalsMap;
  for (pair<unsigned, unordered_set<unsigned>> span : lifespan) {
    vector<unsigned> lines(span.second.begin(), span.second.end());
    sort(lines.begin(), lines.end());
    unsigned left = 0, right = 1;
    while (right < lines.size()) {
      while (right < lines.size() && lines[right - 1] + 1 == lines[right])
        right++;
      intervalsMap[span.first].emplace_back(lines[left], lines[right - 1]);
      left = right;
      right++;
    }
  }
  vector<pair<unsigned, vector<pair<unsigned, unsigned>>>> intervals(
      intervalsMap.begin(), intervalsMap.end());
  for (unsigned i = 0; i < intervals.size(); i++)
    for (unsigned j = i + 1; j < intervals.size(); j++) {
      if (intervals[i].first == intervals[j].first)
        break;
      bool flag = false;
      for (const pair<unsigned, unsigned> &interval1 : intervals[i].second) {
        for (const pair<unsigned, unsigned> &interval2 : intervals[j].second) {
          if (interval1.first > interval2.second ||
              interval1.second < interval2.first)
            continue;
          conflictMap[intervals[i].first].insert(intervals[j].first);
          conflictMap[intervals[j].first].insert(intervals[i].first);
          flag = true;
          break;
        }
        if (flag)
          break;
      }
    }
}

void LinearRegAllocator::calcLifespan() {
  simpleScan();
  for (unordered_map<unsigned, unordered_set<unsigned>>::iterator it =
           lifespan.begin();
       it != lifespan.end(); it++) {
    for (unsigned rId : rMap[it->first]) {
      unordered_set<unsigned> span, track;
      scanSpan(rId, it->first, span, track);
      it->second.insert(span.begin(), span.end());
    }
  }
}

void LinearRegAllocator::calcPrevMap() {
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

Reg::Type LinearRegAllocator::findSReg(unsigned tempId) {
  unordered_set<Reg::Type> conflictSet;
  for (unsigned conflictTempId : conflictMap[tempId]) {
    if (allocatedTemps.find(conflictTempId) == allocatedTemps.end())
      continue;
    if (ftemp2Reg.find(conflictTempId) != ftemp2Reg.end())
      conflictSet.insert(ftemp2Reg[conflictTempId]);
  }
  for (unsigned i = 0; i < 16; i++)
    if (conflictSet.find(sRegs[i]) == conflictSet.end()) {
      usedSRegNum = max(usedSRegNum, i + 1);
      return sRegs[i];
    }
  return Reg::SPILL;
}

Reg::Type LinearRegAllocator::findVReg(unsigned tempId) {
  unordered_set<Reg::Type> conflictSet;
  for (unsigned conflictTempId : conflictMap[tempId]) {
    if (allocatedTemps.find(conflictTempId) == allocatedTemps.end())
      continue;
    if (itemp2Reg.find(conflictTempId) != itemp2Reg.end())
      conflictSet.insert(itemp2Reg[conflictTempId]);
  }
  for (unsigned i = 0; i < 8; i++)
    if (conflictSet.find(vRegs[i]) == conflictSet.end()) {
      usedVRegNum = max(usedVRegNum, i + 1);
      return vRegs[i];
    }
  return Reg::SPILL;
}

unsigned LinearRegAllocator::findSpill(unsigned tempId) {
  unordered_set<unsigned> conflictSet;
  for (unsigned conflictTempId : conflictMap[tempId]) {
    if (allocatedTemps.find(conflictTempId) == allocatedTemps.end())
      continue;
    if (temp2SpillReg.find(conflictTempId) != temp2SpillReg.end())
      conflictSet.insert(temp2SpillReg[conflictTempId]);
  }
  for (unsigned i = 0; i < spillNum; i++)
    if (conflictSet.find(i) == conflictSet.end())
      return i;
  return spillNum++;
}

void LinearRegAllocator::preProcess() {
  unsigned irsSize;
  do {
    irsSize = irs.size();
    unordered_map<IR *, unsigned> irIdMap;
    for (unsigned i = 0; i < irs.size(); i++)
      irIdMap[irs[i]] = i;
    vector<IR *> newIRs;
    for (unsigned i = 0; i < irs.size(); i++) {
      if (irs[i]->items.empty() || (irs[i]->items[0]->type != IRItem::FTEMP &&
                                    irs[i]->items[0]->type != IRItem::ITEMP)) {
        newIRs.push_back(irs[i]);
        continue;
      }
      bool flag = false;
      unordered_set<unsigned> visited;
      vector<unsigned> frontier;
      if (irs[i]->type == IR::GOTO)
        frontier.push_back(irIdMap[irs[i]->items[0]->ir]);
      else if (irs[i]->type == IR::BEQ || irs[i]->type == IR::BGE ||
               irs[i]->type == IR::BGT || irs[i]->type == IR::BLE ||
               irs[i]->type == IR::BLT || irs[i]->type == IR::BNE) {
        frontier.push_back(i + 1);
        frontier.push_back(irIdMap[irs[i]->items[0]->ir]);
      } else
        frontier.push_back(i + 1);
      while (!frontier.empty()) {
        unsigned cur = frontier.back();
        frontier.pop_back();
        if (visited.find(cur) != visited.end())
          continue;
        visited.insert(cur);
        if (cur >= irs.size())
          continue;
        for (unsigned j = 1; j < irs[cur]->items.size(); j++) {
          if (irs[cur]->items[j]->type == IRItem::FTEMP ||
              irs[cur]->items[j]->type == IRItem::ITEMP) {
            if (irs[i]->items[0]->iVal == irs[cur]->items[j]->iVal) {
              flag = true;
              break;
            }
          }
        }
        if (flag)
          break;
        if (irs[cur]->type == IR::GOTO)
          frontier.push_back(irIdMap[irs[cur]->items[0]->ir]);
        else if (irs[cur]->type == IR::BEQ || irs[cur]->type == IR::BGE ||
                 irs[cur]->type == IR::BGT || irs[cur]->type == IR::BLE ||
                 irs[cur]->type == IR::BLT || irs[cur]->type == IR::BNE) {
          frontier.push_back(cur + 1);
          frontier.push_back(irIdMap[irs[cur]->items[0]->ir]);
        } else
          frontier.push_back(cur + 1);
      }
      if (flag)
        newIRs.push_back(irs[i]);
    }
    irs = newIRs;
  } while (irsSize > irs.size());
}

void LinearRegAllocator::scanSpan(unsigned cur, unsigned tId,
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

void LinearRegAllocator::simpleScan() {
  for (unsigned i = 0; i < irs.size(); i++)
    for (unsigned j = 0; j < irs[i]->items.size(); j++)
      if (irs[i]->items[j]->type == IRItem::FTEMP ||
          irs[i]->items[j]->type == IRItem::ITEMP) {
        if (tempType.find(irs[i]->items[j]->iVal) == tempType.end())
          tempType[irs[i]->items[j]->iVal] = irs[i]->items[j]->type;
        lifespan[irs[i]->items[j]->iVal].insert(i);
        if (j)
          rMap[irs[i]->items[j]->iVal].insert(i);
        else
          wMap[irs[i]->items[j]->iVal].insert(i);
      }
}

unordered_map<unsigned, Reg::Type> LinearRegAllocator::getFtemp2Reg() {
  if (!isProcessed)
    allocate();
  return ftemp2Reg;
}

std::vector<IR *> LinearRegAllocator::getIRs() {
  if (!isProcessed)
    allocate();
  return irs;
}

unordered_map<unsigned, Reg::Type> LinearRegAllocator::getItemp2Reg() {
  if (!isProcessed)
    allocate();
  return itemp2Reg;
}

unordered_map<unsigned, unsigned> LinearRegAllocator::getTemp2SpillReg() {
  if (!isProcessed)
    allocate();
  return temp2SpillReg;
}

vector<unsigned> LinearRegAllocator::getUsedRegNum() {
  if (!isProcessed)
    allocate();
  return {usedVRegNum, usedSRegNum, spillNum};
}
