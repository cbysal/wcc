#include "ASMOptimizer.h"
#include "../GlobalData.h"
#include <algorithm>
#include <iostream>
#include <queue>

using namespace std;

const std::vector<Reg::Type> aRegs = {
    Reg::A1,  Reg::A2,  Reg::A3,  Reg::A4,  Reg::S0,  Reg::S1, Reg::S2,
    Reg::S3,  Reg::S4,  Reg::S5,  Reg::S6,  Reg::S7,  Reg::S8, Reg::S9,
    Reg::S10, Reg::S11, Reg::S12, Reg::S13, Reg::S14, Reg::S15};

void ASMOptimizer::optimize() {
  peepholeOptimize();
  // scheduleOptimize();
}

void ASMOptimizer::peepholeOptimize() {
  for (unordered_map<Symbol *, vector<ASM *>>::iterator it = funcASMs.begin();
       it != funcASMs.end(); it++) {
    vector<ASM *> &asms = it->second;
    asms.push_back(new ASM(ASM::NOP, {}));
    asms.push_back(new ASM(ASM::NOP, {}));
    asms.push_back(new ASM(ASM::NOP, {}));
    vector<ASM *> newASMs;
    for (unsigned i = 0; i < asms.size() - 3; i++) {
      if (asms[i]->type == ASM::B && asms[i + 1]->type == ASM::LABEL)
        if (asms[i]->items[0]->iVal == asms[i + 1]->items[0]->iVal)
          continue;
      if (asms[i]->type == ASM::STR && asms[i + 1]->type == ASM::LDR) {
        Reg::Type reg1 = asms[i]->items[1]->reg;
        Reg::Type reg2 = asms[i + 1]->items[1]->reg;
        unsigned offset1 = 0;
        unsigned offset2 = 0;
        if (asms[i]->items.size() == 3)
          offset1 = asms[i]->items[2]->iVal;
        if (asms[i + 1]->items.size() == 3)
          offset2 = asms[i + 1]->items[2]->iVal;
        if (reg1 == reg2 && offset1 == offset2) {
          asms[i + 1]->type = ASM::MOV;
          asms[i + 1]->items.resize(2);
          asms[i + 1]->items[1]->type = ASMItem::REG;
          asms[i + 1]->items[1]->reg = asms[i]->items[0]->reg;
        }
      }
      if (asms[i]->type == ASM::MOV && asms[i + 1]->type == ASM::MOV) {
        if (asms[i]->items[0]->type == ASMItem::REG &&
            asms[i]->items[1]->type == ASMItem::INT &&
            asms[i + 1]->items[0]->type == ASMItem::REG &&
            asms[i + 1]->items[1]->type == ASMItem::REG &&
            asms[i]->items[0]->reg == asms[i + 1]->items[1]->reg) {
          asms[i + 1]->items[1]->type = ASMItem::INT;
          asms[i + 1]->items[1]->iVal = asms[i]->items[1]->iVal;
        }
        if (asms[i]->items[0]->type == ASMItem::REG &&
            asms[i]->items[1]->type == ASMItem::REG &&
            asms[i + 1]->items[0]->type == ASMItem::REG &&
            asms[i + 1]->items[1]->type == ASMItem::REG &&
            asms[i]->items[0]->reg == asms[i + 1]->items[1]->reg &&
            asms[i]->items[1]->reg == asms[i + 1]->items[0]->reg) {
          newASMs.push_back(asms[i]);
          i++;
          continue;
        }
      }
      if (asms[i]->type == ASM::MOV && asms[i + 1]->type == ASM::STR &&
          asms[i + 2]->type == ASM::MOV) {
        if (asms[i]->items[0]->reg == asms[i + 2]->items[0]->reg &&
            asms[i]->items[1]->iVal == asms[i + 2]->items[1]->iVal) {
          newASMs.push_back(asms[i]);
          newASMs.push_back(asms[i + 1]);
          i += 2;
          continue;
        }
      }
      if (asms[i]->type == ASM::MVN && asms[i + 1]->type == ASM::MOV) {
        if (asms[i]->items[0]->type == ASMItem::REG &&
            asms[i]->items[1]->type == ASMItem::INT &&
            asms[i + 1]->items[0]->type == ASMItem::REG &&
            asms[i + 1]->items[1]->type == ASMItem::REG &&
            asms[i]->items[0]->reg == asms[i + 1]->items[1]->reg) {
          asms[i + 1]->type = ASM::MVN;
          asms[i + 1]->items[1]->type = ASMItem::INT;
          asms[i + 1]->items[1]->iVal = asms[i]->items[1]->iVal;
        }
      }
      newASMs.push_back(asms[i]);
    }
    while (!newASMs.empty() && newASMs.back()->type == ASM::NOP)
      newASMs.pop_back();
    asms = newASMs;
  }
}

void buildGraph(vector<vector<unsigned>> &sons, vector<unsigned> &heights,
                vector<bool> &isMem, AsmBasicBlock &bb, vector<ASM *> &asms) {
  unordered_map<Reg::Type, unsigned> lastWrite;
  unordered_map<Reg::Type, vector<unsigned>> lastRead;
  unsigned lastMem;
  for (unsigned r = Reg::A1; r <= Reg::S31; r++) {
    lastWrite[static_cast<Reg::Type>(r)] = asms.size();
    lastMem = asms.size();
  }
  for (int i = static_cast<int>(bb.last); static_cast<int>(bb.first) <= i;
       i--) {
    auto items = asms[i]->items;
    if (asms[i]->type == ASM::BL) {
      // read mem
      sons[i].emplace_back(lastMem);
      heights[i] = std::max(heights[i], heights[lastMem] + 1);
      lastMem = i;
      // write s0 and a1 and sp
      sons[i].insert(sons[i].end(), lastRead[Reg::A1].begin(),
                     lastRead[Reg::A1].end());
      for (unsigned rd : lastRead[Reg::A1])
        heights[i] = std::max(heights[i], heights[rd] + 1);
      lastRead[Reg::A1].clear();
      sons[i].insert(sons[i].end(), lastRead[Reg::S0].begin(),
                     lastRead[Reg::S0].end());
      for (unsigned rd : lastRead[Reg::S0])
        heights[i] = std::max(heights[i], heights[rd] + 1);
      lastRead[Reg::S0].clear();
      sons[i].insert(sons[i].end(), lastRead[Reg::SP].begin(),
                     lastRead[Reg::SP].end());
      for (unsigned rd : lastRead[Reg::SP])
        heights[i] = std::max(heights[i], heights[rd] + 1);
      lastRead[Reg::SP].clear();
      // read params
      for (auto r : aRegs) {
        sons[i].emplace_back(lastWrite[r]);
        heights[i] = std::max(heights[i], heights[lastWrite[r]] + 1);
        lastRead[r].emplace_back(i);
      }
      lastWrite[Reg::A1] = i;
      lastWrite[Reg::S0] = i;
    } else if (asms[i]->type == ASM::STR || asms[i]->type == ASM::VSTR) {
      sons[i].emplace_back(lastWrite[items[0]->reg]);
      sons[i].emplace_back(lastMem);
      heights[i] = std::max(heights[i], heights[lastWrite[items[0]->reg]] + 1);
      heights[i] = std::max(heights[i], heights[lastMem] + 1);
      lastRead[items[0]->reg].emplace_back(i);
      lastMem = i;
      isMem[i] = true;
    } else if (asms[i]->type == ASM::LDR || asms[i]->type == ASM::VLDR) {
      sons[i].emplace_back(lastWrite[items[0]->reg]);
      sons[i].emplace_back(lastMem);
      heights[i] = std::max(heights[i], heights[lastWrite[items[0]->reg]] + 1);
      heights[i] = std::max(heights[i], heights[lastMem] + 1);
      // add all read after write
      sons[i].insert(sons[i].end(), lastRead[items[0]->reg].begin(),
                     lastRead[items[0]->reg].end());
      for (unsigned rd : lastRead[items[0]->reg])
        heights[i] = std::max(heights[i], heights[rd] + 1);
      lastRead[items[0]->reg].clear();
      // update
      lastWrite[items[0]->reg] = i;
      lastMem = i;
      isMem[i] = true;
    } else if (!items.empty() && items[0]->type == ASMItem::REG) {
      sons[i].emplace_back(lastWrite[items[0]->reg]);
      heights[i] = std::max(heights[i], heights[lastWrite[items[0]->reg]] + 1);
      // add all read after write
      sons[i].insert(sons[i].end(), lastRead[items[0]->reg].begin(),
                     lastRead[items[0]->reg].end());
      for (unsigned rd : lastRead[items[0]->reg])
        heights[i] = std::max(heights[i], heights[rd] + 1);
      lastRead[items[0]->reg].clear();
      // update
      lastWrite[items[0]->reg] = i;
    }
    for (size_t j = 1; j < items.size(); j++) {
      if (items[j]->type == ASMItem::REG) {
        if (static_cast<int>(lastWrite[items[j]->reg]) == i)
          continue;
        sons[i].emplace_back(lastWrite[items[j]->reg]);
        heights[i] =
            std::max(heights[i], heights[lastWrite[items[j]->reg]] + 1);
        lastRead[items[j]->reg].emplace_back(i);
      }
    }
  }
}

void topsortInBlock(AsmBasicBlock &bb, vector<vector<unsigned>> &sons,
                    vector<vector<unsigned>> &parents,
                    vector<unsigned> &heights, vector<unsigned> &ordered) {
  auto cmp = [&heights](unsigned a, unsigned b) -> bool {
    return heights[a] < heights[b];
  };
  vector<unsigned> degs(sons.size());
  priority_queue<unsigned, vector<unsigned>, decltype(cmp)> que(cmp);
  for (unsigned i = bb.first; i <= bb.last; i++) {
    degs[i] = parents[i].size();
  }
  for (unsigned i = bb.first; i <= bb.last; i++) {
    if (parents[i].empty())
      que.push(i);
  }
  int k = 0;
  while (!que.empty()) {
    unsigned x = que.top();
    que.pop();
    ordered[k++] = x;
    for (auto y : sons[x]) {
      if (y == sons.size() - 1)
        continue;
      degs[y]--;
      if (degs[y] == 0)
        que.push(y);
    }
  }
}

void getTimestamp(vector<vector<unsigned>> &parents, vector<unsigned> &ordered,
                  vector<ASM *> &asms, vector<bool> &isMem,
                  vector<int> &times) {
  int ts = 0;
  vector<bool> occupied(3 * ordered.size());
  for (size_t i = 0; i < ordered.size(); i++) {
    unsigned x = ordered[i];
    while (occupied[ts])
      ts++;
    int tts = ts;
    for (size_t j = 0; j < parents[x].size(); j++) {
      tts = std::max(ts, times[parents[x][j]] + isMem[parents[x][j]] ? 2 : 1);
    }
    while (occupied[tts])
      tts++;
    times[x] = tts;
    occupied[tts] = true;
  }
}

void ASMOptimizer::scheduleOptimize() {
  for (auto &it : funcASMs) {
    std::vector<AsmBasicBlock> bbs = buildBlock(it.second);
    vector<vector<unsigned>> sons(it.second.size() + 1);
    vector<vector<unsigned>> parents(it.second.size() + 1);
    vector<unsigned> heights(it.second.size() + 1);
    vector<bool> isMem(it.second.size());
    for (AsmBasicBlock bbp : bbs) {
      buildGraph(sons, heights, isMem, bbp, it.second);
      for (size_t i = 0; i < sons.size(); i++) {
        for (size_t j = 0; j < sons[i].size(); j++) {
          parents[sons[i][j]].emplace_back(i);
        }
      }
      vector<unsigned> ordered(bbp.last - bbp.first + 1);
      vector<int> times(it.second.size());
      topsortInBlock(bbp, sons, parents, heights, ordered);
      getTimestamp(parents, ordered, it.second, isMem, times);
      // deal with the head codes and tail codes
      for (size_t i = bbp.first; i <= bbp.last; i++) {
        if (it.second[i]->type == ASM::LABEL ||
            it.second[i]->type == ASM::PUSH || it.second[i]->type == ASM::VPUSH)
          times[i] = -100000 + static_cast<int>(i - bbp.first);
        else
          break;
      }
      for (int i = static_cast<int>(bbp.last); static_cast<int>(bbp.first) <= i;
           i--) {
        ASM::ASMOpType tp = it.second[i]->type;
        if (tp == ASM::B || tp == ASM::CMP || tp == ASM::VCMP ||
            tp == ASM::VMRS || tp == ASM::POP || tp == ASM::VPOP)
          times[i] = static_cast<int>(3 * ordered.size()) + i;
        else
          break;
      }
      std::sort(ordered.begin(), ordered.end(),
                [&times](unsigned a, unsigned b) -> bool {
                  return times[a] < times[b];
                });
      // std::cout << bbp.bid << " ordered \n";
      // for (size_t i = 0; i < ordered.size(); i++) {
      //   std::cout << it.second[ordered[i]]->toString() << '\n';
      // }
      // bbp.display(it.second);
      // schedule the asms
      vector<ASM *> tempAsms(bbp.last - bbp.first + 1);
      for (size_t i = 0; i < ordered.size(); i++)
        tempAsms[i] = it.second[ordered[i]];
      for (unsigned i = 0; i < tempAsms.size(); i++)
        it.second[i + bbp.first] = tempAsms[i];
    }
    bbs.clear();
  }
}

std::vector<AsmBasicBlock> ASMOptimizer::buildBlock(std::vector<ASM *> asms) {
  unordered_map<int, unsigned> labelMap;
  set<unsigned> startPoints;
  startPoints.emplace(0);
  for (size_t i = 0; i < asms.size(); i++) {
    if (asms[i]->type == ASM::LABEL)
      labelMap[asms[i]->items[0]->iVal] = i;
  }
  for (size_t i = 0; i < asms.size(); i++) {
    // if (asms[i]->type == ASM::B && asms[i]->cond == ASM::AL) {
    //   int x = labelMap[asms[i]->items[0]->iVal];
    //   if (startPoints.count(x) == 0)
    //     startPoints.emplace(x);
    // } else
    if (asms[i]->type == ASM::B) {
      int x = labelMap[asms[i]->items[0]->iVal];
      if (startPoints.count(x) == 0)
        startPoints.emplace(x);
      if (startPoints.count(i + 1) == 0)
        startPoints.emplace(i + 1);
    }
  }

  vector<AsmBasicBlock> bbs;
  for (auto sp : startPoints) {
    if (sp == asms.size())
      continue;
    bbs.emplace_back(sp);
    bbs.back().bid = bbs.size() - 1;
  }
  for (size_t i = 0; i < bbs.size(); i++) {
    unsigned sp = bbs[i].first + 1;
    while (sp < asms.size() && startPoints.count(sp) == 0) {
      sp++;
    }
    bbs[i].last = sp - 1;
  }
  return bbs;
}