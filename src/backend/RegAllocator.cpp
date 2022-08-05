#include <algorithm>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <unordered_set>

#include "RegAllocator.h"

using namespace std;

RegAllocator::RegAllocator(const vector<IR *> &irs) {
  this->irs = irs;
  this->isProcessed = false;
  this->regs = new RegFile();
  // this->coloring = false;
}

RegAllocator::~RegAllocator() { delete regs; }

void RegAllocator::allocate() {
  isProcessed = true;
  calcPrevMap();
  simpleScan();
  calcLifespan();
  // betterAllocate();
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
    // allocate();
    betterAllocate();
  return ftemp2Reg;
}

unordered_map<unsigned, ASMItem::RegType> RegAllocator::getItemp2Reg() {
  if (!isProcessed)
    // allocate();
    betterAllocate();
  return itemp2Reg;
}

unordered_map<unsigned, unsigned> RegAllocator::getTemp2SpillReg() {
  if (!isProcessed)
    // allocate();
    betterAllocate();
  return temp2SpillReg;
}

vector<unsigned> RegAllocator::getUsedRegNum() {
  if (!isProcessed)
    // allocate();
    betterAllocate();
  return {regs->getUsed(RegFile::V), regs->getUsed(RegFile::S),
          regs->getUsed(RegFile::SPILL)};
}

// ADD allocate registers using graph coloring
void RegAllocator::betterAllocate() {
  isProcessed = true;
  buildBlocks();
  unsigned maxTemp;
  calcUseAndDef(maxTemp);
  interfereMatrix = vector<set<unsigned>>(maxTemp + 1);
  useCount = vector<int>(maxTemp + 1);
  std::vector<unsigned> path(1, 0);
  dfsTestLoop(path);
  topSortBlocks();
  calcInAndOut();
  int spillCount = 0;
  unsigned vCount = 0, sCount = 0;
  calcInterfereByLine(spillCount);
  vCount = graphColoring(IRItem::ITEMP, spillCount);
  sCount = graphColoring(IRItem::FTEMP, spillCount);
  regs->setUsed(RegFile::V, vCount);
  regs->setUsed(RegFile::S, sCount);
  regs->setUsed(RegFile::SPILL, spillCount);
  // std::cout << "Regs use " << vCount << ' ' << sCount << ' ' << spillCount
  //           << std::endl;
  // for (size_t i = 0; i < bbs.size(); i++) {
  //   bbs[i]->display(irs);
  // }
  // logInterfere();
  // itemp2Reg.clear();
  // ftemp2Reg.clear();
  // temp2SpillReg.clear();
}

// Function to build the basic blocks
void RegAllocator::buildBlocks() {
  unordered_map<IR *, unsigned> irIdMap;
  map<unsigned, unsigned> startPoints;
  startPoints[0] = 0;
  for (size_t i = 0; i < irs.size(); i++) {
    irIdMap[irs[i]] = i;
  }
  for (size_t i = 0; i < irs.size(); i++) { // Find the start points for bb
    if (irs[i]->type == IR::IRType::GOTO) {
      unsigned x = irIdMap[irs[i]->items[0]->ir];
      if (startPoints.find(x) == startPoints.end())
        startPoints[x] = 0;
    } else if (irs[i]->type == IR::BGE || irs[i]->type == IR::BGT ||
               irs[i]->type == IR::BEQ || irs[i]->type == IR::BNE ||
               irs[i]->type == IR::BLE || irs[i]->type == IR::BLT) {
      unsigned x = irIdMap[irs[i]->items[0]->ir];
      if (startPoints.find(x) == startPoints.end())
        startPoints[x] = 0;
      if (startPoints.find(i + 1) == startPoints.end())
        startPoints[i + 1] = 0;
    }
  }
  for (auto &sp : startPoints) {
    if (sp.first == irs.size())
      continue;
    sp.second = bbs.size();
    bbs.emplace_back(new BasicBlock{sp.first});
    bbs.back()->bid = bbs.size() - 1;
  }
  // Get the range of bbs and get the predecessors and successors
  for (size_t i = 0; i < bbs.size(); i++) {
    auto sp = bbs[i]->first + 1;
    // std::cout << sp << std::endl;
    while (sp < irs.size() && startPoints.find(sp) == startPoints.end()) {
      sp++;
    }
    bbs[i]->last = sp - 1;
    if (irs.size() <= sp)
      continue;
    IR::IRType tp = irs[sp - 1]->type;
    if (tp != IR::GOTO) {
      unsigned sc0 = startPoints[sp]; // ordinary successor
      bbs[i]->addSucc(bbs[sc0]);
      bbs[sc0]->addPred(bbs[i]);
    }
    if (tp == IR::GOTO || tp == IR::BGE || tp == IR::BGT || tp == IR::BEQ ||
        tp == IR::BNE || tp == IR::BLE || tp == IR::BLT) {
      unsigned jmp = irIdMap[irs[sp - 1]->items[0]->ir];
      unsigned sc1 = startPoints[jmp]; // jump successor
      bbs[i]->addSucc(bbs[sc1]);
      bbs[sc1]->addPred(bbs[i]);
    }
  }
}

// Get the use and def set for bbs
void RegAllocator::calcUseAndDef(unsigned &maxTemp) {
  maxTemp = 0;
  for (size_t i = 0; i < bbs.size(); i++) {
    for (unsigned j = bbs[i]->first; j <= bbs[i]->last; j++) {
      if (irs[j]->items.empty())
        continue;
      if ((irs[j]->items[0]->type == IRItem::ITEMP ||
           irs[j]->items[0]->type == IRItem::FTEMP) &&
          bbs[i]->useTemps.count(irs[j]->items[0]->iVal) == 0) {
        bbs[i]->defTemps.insert(irs[j]->items[0]->iVal);
        if (irs[j]->items[0]->type == IRItem::ITEMP)
          iTemps.emplace(irs[j]->items[0]->iVal);
        maxTemp =
            std::max(maxTemp, static_cast<unsigned>(irs[j]->items[0]->iVal));
      }
      for (size_t k = 1; k < irs[j]->items.size(); k++) {
        if (irs[j]->items[k]->type == IRItem::ITEMP ||
            irs[j]->items[k]->type == IRItem::FTEMP) {
          bbs[i]->useTemps.insert(irs[j]->items[k]->iVal);
          // calc the max id for int and float temp
          maxTemp =
              std::max(maxTemp, static_cast<unsigned>(irs[j]->items[k]->iVal));
          // add the int temp to the set
          if (irs[j]->items[k]->type == IRItem::ITEMP)
            iTemps.emplace(irs[j]->items[k]->iVal);
        }
      }
    }
  }
}

void RegAllocator::topSortBlocks() {
  const int maxn = 0x7fffffff;
  std::vector<BasicBlock *> topbbs;
  std::vector<int> degs(bbs.size());

  for (size_t i = 0; i < bbs.size(); i++) {
    degs[i] = bbs[i]->succs.size();
  }

  while (topbbs.size() != bbs.size()) {
    int cur = 0;
    for (size_t i = 0; i < degs.size(); i++) {
      if (degs[i] <= degs[cur] ||
          (bbs[i]->inLoop && !bbs[cur]->inLoop && degs[i] != maxn))
        cur = i;
    }
    degs[cur] = maxn;
    for (BasicBlock *bb : bbs[cur]->preds) {
      if (degs[bb->bid] != maxn) {
        degs[bb->bid]--;
      }
    }
    topbbs.emplace_back(bbs[cur]);
  }

  bbs.clear();
  bbs = topbbs;
  for (size_t i = 0; i < bbs.size(); i++) {
    bbs[i]->bid = i;
  }
  return;
}

// Itetate to calc Ins and Outs for every blocks
void RegAllocator::calcInAndOut() {
  while (true) {
    bool still = true;
    for (BasicBlock *bb : bbs) {
      still = still && bb->updateOut();
      still = still && bb->updateIn();
    }
    if (still)
      break;
  }
  return;
}

// Dfs to test whether a block is in loop
void RegAllocator::dfsTestLoop(std::vector<unsigned> &path) {
  unsigned cur = path.back();
  if (bbs[cur]->onPath) {
    bbs[cur]->inLoop = true;
    for (int i = path.size() - 2; 0 <= i; i--) {
      if (path[i] == cur)
        break;
      bbs[path[i]]->inLoop = true;
    }
  } else {
    bbs[cur]->visit = true;
    for (BasicBlock *bb : bbs[cur]->succs) {
      if (bb->visit)
        continue;
      path.emplace_back(bb->bid);
      bb->onPath = true;
      dfsTestLoop(path);
      path.pop_back();
      bb->onPath = false;
    }
  }
  return;
}

void RegAllocator::calcInterfere() {
  for (BasicBlock *bb : bbs) {
    std::vector<int> used;
    std::unordered_map<int, int> idMap;
    std::vector<std::vector<std::pair<unsigned, unsigned>>> ranges;
    std::vector<unsigned> lastUse, lastDef;
    // get the used temps in the block
    for (unsigned i : bb->useTemps) {
      if (idMap.find(i) == idMap.end()) {
        idMap[i] = used.size();
        used.emplace_back(i);
      }
    }
    // take out temps into account
    for (unsigned i : bb->outTemps) {
      if (idMap.find(i) == idMap.end()) {
        idMap[i] = used.size();
        used.emplace_back(i);
      }
    }
    lastUse = std::vector<unsigned>(used.size());
    lastDef = std::vector<unsigned>(used.size());
    ranges =
        std::vector<std::vector<std::pair<unsigned, unsigned>>>{used.size()};
    // get the ranges for each temp
    for (size_t i = bb->first; i <= bb->last; i++) {
      auto &its = irs[i]->items;
      if (its.empty())
        continue;
      // items[0] for def
      if ((its[0]->type == IRItem::ITEMP || its[0]->type == IRItem::ITEMP) &&
          idMap.find(its[0]->iVal) != idMap.end()) {
        int id = idMap[its[0]->iVal];
        if (lastDef[id] < lastUse[id]) {
          ranges[id].emplace_back(lastDef[id] + 1, lastUse[id]);
        }
        lastDef[id] = i - bb->first + 1;
      }
      // other items for use
      for (size_t j = 1; j < its.size(); j++) {
        if (its[j]->type == IRItem::ITEMP || its[j]->type == IRItem::FTEMP) {
          int id = idMap[its[j]->iVal];
          lastUse[id] = i - bb->first + 1;
          useCount[its[j]->iVal] += bb->inLoop ? 10 : 1;
        }
      }
    }
    // treat out temps as use
    for (unsigned i : bb->outTemps) {
      int id = idMap[i];
      ranges[id].emplace_back(lastDef[id] + 1, bb->last - bb->first + 1);
      lastDef[id] = bb->last - bb->first + 1;
    }
    // deal the temps that are not encapsulated
    for (size_t i = 0; i < used.size(); i++) {
      if (lastDef[i] < lastUse[i])
        ranges[i].emplace_back(lastDef[i] + 1, lastUse[i]);
    }
    updateInterfereMatrix(used, ranges);
  } // end for bb
}

void RegAllocator::updateInterfereMatrix(
    const vector<int> &used,
    const vector<vector<pair<unsigned, unsigned>>> &ranges) {
  for (size_t i = 0; i < used.size(); i++) {
    int x = used[i];
    const vector<pair<unsigned, unsigned>> &rgx = ranges[i];
    bool xIsInt = iTemps.find(x) != iTemps.end();
    for (size_t j = i + 1; j < used.size(); j++) {
      int y = used[j];
      // test if x and y are both int or float
      if (xIsInt ^ (iTemps.find(y) != iTemps.end()))
        continue;
      const vector<pair<unsigned, unsigned>> &rgy = ranges[j];
      bool conf = false;
      for (auto p : rgx) {
        for (auto q : rgy) {
          if (q.second < p.first || p.second < q.first)
            continue;
          conf = true;
          break;
        }
        if (conf)
          break;
      }
      if (conf) {
        this->interfereMatrix[x].emplace(y);
        this->interfereMatrix[y].emplace(x);
        break;
      }
    }
  }
}

void RegAllocator::logInterfere() {
  std::cout << "Interfere\n";
  for (size_t i = 0; i < interfereMatrix.size(); i++) {
    // if (interfereMatrix[i].empty())
    //   continue;
    std::cout << i << ": ";
    std::cout << "Reg: ";
    if (itemp2Reg.count(i) != 0)
      std::cout << itemp2Reg[i];
    else if (ftemp2Reg.count(i) != 0)
      std::cout << ftemp2Reg[i];
    else
      std::cout << "spill" << temp2SpillReg[i];
    for (unsigned x : interfereMatrix[i]) {
      std::cout << ' ' << x;
    }
    std::cout << std::endl;
  }
}

// calc the interfere relation by each line
void RegAllocator::calcInterfereByLine(int &spillCount) {
  for (auto bb : bbs) {
    set<unsigned> comAlive;
    set<unsigned> alive;
    for (unsigned t : bb->outTemps) {
      comAlive.emplace(t);
    }
    // std::cerr << bb->last - bb->first << std::endl;
    for (int i = (int)bb->last; (int)bb->first <= i; i--) {
      vector<IRItem *> &its = irs[i]->items;
      if (its.empty())
        continue;
      if (its[0]->type == IRItem::ITEMP || its[0]->type == IRItem::FTEMP) {
        // TODO: delete the judge after un used temps are deleted
        if (alive.count(its[0]->iVal) != 0 ||
            comAlive.count(its[0]->iVal) != 0) {
          alive.erase(its[0]->iVal);
          comAlive.erase(its[0]->iVal);
        } else {
          comAlive.emplace(its[0]->iVal);
          useCount[its[0]->iVal] += 1;
        }
        // alive.erase(its[0]->iVal);
        // comAlive.erase(its[0]->iVal);
      }
      for (size_t j = 1; j < its.size(); j++) {
        if (its[j]->type == IRItem::ITEMP || its[j]->type == IRItem::FTEMP) {
          comAlive.emplace(its[j]->iVal);
          useCount[its[j]->iVal] += bb->inLoop ? 10 : 1;
        }
      }
      if (0 < comAlive.size())
        updateInterfereMatrix(comAlive, alive, spillCount);
      alive.insert(comAlive.begin(), comAlive.end());
      comAlive.clear();
    }
  }
}

void RegAllocator::updateInterfereMatrix(const set<unsigned> &comAlive,
                                         const set<unsigned> &alive,
                                         int &spillCount) {
  vector<unsigned> newlive(comAlive.begin(), comAlive.end());
  vector<unsigned> live(alive.begin(), alive.end());
  // std::cerr << newlive.size() << ' ' << live.size() << std::endl;
  for (size_t i = 0; i < newlive.size(); i++) {
    // directly spill the temps with too many interfefe edges
    if (500 < live.size()) {
      temp2SpillReg[newlive[i]] = spillCount++;
      continue;
    }
    for (size_t j = i + 1; j < newlive.size(); j++) {
      bool ii = iTemps.find(newlive[i]) != iTemps.end();
      bool jni = iTemps.find(newlive[j]) == iTemps.end();
      if (ii ^ jni) {
        interfereMatrix[newlive[i]].emplace(newlive[j]);
        interfereMatrix[newlive[j]].emplace(newlive[i]);
      }
    }
    for (size_t j = 0; j < live.size(); j++) {
      bool ii = iTemps.find(newlive[i]) != iTemps.end();
      bool jni = iTemps.find(live[j]) == iTemps.end();
      if (ii ^ jni) {
        interfereMatrix[newlive[i]].emplace(live[j]);
        interfereMatrix[live[j]].emplace(newlive[i]);
      }
    }
  }
}

unsigned RegAllocator::graphColoring(IRItem::IRItemType iorf, int &spillCount) {
  vector<vector<unsigned>> inters(interfereMatrix.size());
  vector<unordered_map<ASMItem::RegType, bool>> color(interfereMatrix.size());
  vector<ASMItem::RegType> regrs;
  unordered_map<unsigned, ASMItem::RegType> *temp2Reg;
  unordered_set<unsigned> usedRegSet;
  vector<int> degs(inters.size(), 0);
  // check iorf is ITEMP or FTEMP
  if (iorf == IRItem::ITEMP) {
    regrs = regs->getRegs(RegFile::V);
    temp2Reg = &this->itemp2Reg;
  } else {
    regrs = regs->getRegs(RegFile::S);
    temp2Reg = &this->ftemp2Reg;
  }
  regrs = vector<ASMItem::RegType>(regrs.rbegin(), regrs.rend());
  // copy the interfere list
  for (size_t i = 0; i < interfereMatrix.size(); i++) {
    for (unsigned x : interfereMatrix[i])
      inters[i].emplace_back(x);
    degs[i] = inters[i].size();
  }
  // calc the spill score
  vector<double> score(degs.size());
  for (size_t i = 0; i < score.size(); i++) {
    score[i] = useCount[i] * 1. / (degs[i] + 1);
  }
  vector<unsigned> candi(inters.size());
  for (size_t i = 0; i < candi.size(); i++) {
    candi[i] = i;
  }
  std::sort(
      candi.begin(), candi.end(),
      [&score](unsigned a, unsigned b) -> bool { return score[a] < score[b]; });

  // define the priority queue
  auto cmp = [](pair<unsigned, int> &a, pair<unsigned, int> &b) -> bool {
    return b.second < a.second;
  };
  priority_queue<pair<unsigned, int>, vector<pair<unsigned, int>>,
                 decltype(cmp)>
      pque(cmp);
  vector<pair<unsigned, bool>> stk;
  vector<unsigned> spills;
  // directly spill temps with too many interfering neighbors
  for (size_t i = 0; i < degs.size(); i++) {
    if (((iTemps.count(i) == 0) ^ (iorf != IRItem::ITEMP)) || degs[i] < 500)
      continue;
    spills.emplace_back(i);
    degs[i] = -1;
    for (unsigned x : inters[i])
      if (degs[x] != -1)
        degs[x]--;
  }
  for (size_t i = 0; i < degs.size(); i++) {
    if (((iTemps.count(i) != 0) ^ (iorf != IRItem::ITEMP)) &&
        useCount[i] != 0 && degs[i] != -1 && temp2SpillReg.count(i) == 0)
      pque.emplace(i, degs[i]);
  }
  // pre color and spill
  unsigned nxtSpill = 0;
  while (!pque.empty()) {
    pair<unsigned, int> cur = pque.top();
    if (cur.second != degs[cur.first]) {
      pque.pop();
      continue;
    }
    if (cur.second < static_cast<int>(regrs.size())) {
      pque.pop();
      stk.emplace_back(cur.first, false);
      degs[cur.first] = -1;
      for (unsigned x : inters[cur.first]) {
        if (degs[x] == -1) // has been pushed to stk
          continue;
        degs[x]--;
        pque.emplace(x, degs[x]);
      }
    } else {
      while (nxtSpill < candi.size() && degs[candi[nxtSpill]] == -1)
        nxtSpill++;
      stk.emplace_back(candi[nxtSpill], true);
      degs[candi[nxtSpill]] = -1;
      for (unsigned x : inters[candi[nxtSpill]]) {
        if (degs[x] == -1) // has been pushed to stk
          continue;
        degs[x]--;
        pque.emplace(x, degs[x]);
      }
    }
  }
  // true color and spill
  while (!stk.empty()) {
    pair<unsigned, bool> cur = stk.back();
    stk.pop_back();
    unsigned tid = cur.first;
    bool spill = cur.second;
    bool find = false;
    for (size_t i = 0; !find && i < regrs.size(); i++) {
      if (color[tid].find(regrs[i]) != color[tid].end())
        continue;
      (*temp2Reg)[tid] = regrs[i];
      usedRegSet.emplace(regrs[i]);
      for (unsigned x : inters[tid]) {
        color[x][regrs[i]] = true;
      }
      find = true;
    }
    if (spill && !find) {
      spills.emplace_back(tid);
    }
  }
  if (!spills.empty())
    constrainSpill(spills, spillCount);
  return usedRegSet.size();
}

// try to color the spilled temps with the limit number of mem
bool RegAllocator::testSpill(const vector<vector<unsigned>> &inter,
                             vector<int> degs, int base, int limit, bool update,
                             const vector<unsigned> *temps) {
  bool ret = true;
  auto cmp = [](pair<int, int> &a, pair<int, int> &b) -> bool {
    return b.second < a.second;
  };
  vector<unordered_map<int, bool>> spillColor(inter.size());
  priority_queue<pair<int, int>, vector<pair<int, int>>, decltype(cmp)> pque(
      cmp);
  vector<int> stk;

  for (size_t i = 0; i < degs.size(); i++) {
    pque.emplace(i, degs[i]);
  }

  // pre color
  while (!pque.empty()) {
    pair<int, int> cur = pque.top();
    pque.pop();
    if (cur.second != degs[cur.first])
      continue;
    stk.emplace_back(cur.first);
    degs[cur.first] = -1;
    for (unsigned x : inter[cur.first]) {
      if (degs[x] == -1)
        continue;
      degs[x]--;
      pque.emplace(x, degs[x]);
    }
  }
  while (ret && !stk.empty()) {
    int cur = stk.back();
    stk.pop_back();
    bool find = false;
    for (int c = base; !find && c < limit; c++) {
      if (spillColor[cur].count(c) != 0)
        continue;
      find = true;
      if (update) {
        unsigned x = (*temps)[cur];
        temp2SpillReg[x] = c;
      }
      for (unsigned x : inter[cur])
        spillColor[x][c] = true;
    }
    if (!find)
      ret = false;
  }
  return ret;
}

void RegAllocator::constrainSpill(vector<unsigned> &spilled, int &spillCount) {
  int cnt = 0;
  unordered_map<unsigned, int> spillMap;
  for (unsigned x : spilled)
    spillMap[x] = cnt++;
  // get the interfere relationship amoung the spilled temps
  vector<vector<unsigned>> inter(cnt);
  vector<int> degs(cnt);
  for (unsigned x : spilled) {
    int xi = spillMap[x];
    for (unsigned y : interfereMatrix[x]) {
      if (spillMap.count(y) == 0)
        continue;
      inter[xi].emplace_back(spillMap[y]);
    }
    degs[xi] = inter[xi].size();
  }

  // binary search to find the least spill memory needed
  int hi = spillCount + static_cast<int>(spilled.size());
  // int lo = spillCount;
  // while (lo + 1 != hi) {
  //   int mi = (lo + hi) >> 1;
  //   if (testSpill(inter, degs, spillCount, mi))
  //     hi = mi;
  //   else
  //     lo = mi;
  // }
  // std::cerr << hi << ' ' << spilled.size() << std::endl;
  testSpill(inter, degs, spillCount, hi, true, &spilled);
  spillCount = hi;
}