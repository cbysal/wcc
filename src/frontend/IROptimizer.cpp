#include <algorithm>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <unordered_set>

#include "../GlobalData.h"
#include "IROptimizer.h"

using namespace std;

void IROptimizer::assignPass() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    vector<IR *> &irs = it->second;
    unsigned left = 0, right = 0;
    while (right < irs.size()) {
      while (right < irs.size()) {
        if (irs[right]->type == IR::BEQ || irs[right]->type == IR::BGE ||
            irs[right]->type == IR::BGT || irs[right]->type == IR::BLE ||
            irs[right]->type == IR::BLT || irs[right]->type == IR::BNE ||
            irs[right]->type == IR::CALL || irs[right]->type == IR::GOTO ||
            irs[right]->type == IR::LABEL)
          break;
        right++;
      }
      if (right < irs.size())
        right++;
      unordered_map<unsigned, unsigned> itempAssign;
      unordered_map<unsigned, unsigned> ftempAssign;
      for (unsigned i = left; i < right; i++) {
        for (unsigned j = 1; j < irs[i]->items.size(); j++) {
          if (irs[i]->items[j]->type == IRItem::FTEMP &&
              ftempAssign.find(irs[i]->items[j]->iVal) != ftempAssign.end())
            irs[i]->items[j]->iVal = ftempAssign[irs[i]->items[j]->iVal];
          else if (irs[i]->items[j]->type == IRItem::ITEMP &&
                   itempAssign.find(irs[i]->items[j]->iVal) !=
                       itempAssign.end())
            irs[i]->items[j]->iVal = itempAssign[irs[i]->items[j]->iVal];
        }
        if (!irs[i]->items.empty()) {
          if (irs[i]->items[0]->type == IRItem::FTEMP) {
            if (irs[i]->type == IR::MOV &&
                irs[i]->items[1]->type == IRItem::FTEMP) {
              ftempAssign[irs[i]->items[0]->iVal] = irs[i]->items[1]->iVal;
            } else
              ftempAssign.erase(irs[i]->items[0]->iVal);
            unordered_set<unsigned> eraseSet;
            for (unordered_map<unsigned, unsigned>::iterator it =
                     ftempAssign.begin();
                 it != ftempAssign.end(); it++) {
              if (it->second == (unsigned)irs[i]->items[0]->iVal)
                eraseSet.insert(it->first);
            }
            for (unsigned eraseItem : eraseSet)
              ftempAssign.erase(eraseItem);
          } else if (irs[i]->items[0]->type == IRItem::ITEMP) {
            if (irs[i]->type == IR::MOV &&
                irs[i]->items[1]->type == IRItem::ITEMP) {
              itempAssign[irs[i]->items[0]->iVal] = irs[i]->items[1]->iVal;
            } else
              itempAssign.erase(irs[i]->items[0]->iVal);
            unordered_set<unsigned> eraseSet;
            for (unordered_map<unsigned, unsigned>::iterator it =
                     itempAssign.begin();
                 it != itempAssign.end(); it++) {
              if (it->second == (unsigned)irs[i]->items[0]->iVal)
                eraseSet.insert(it->first);
            }
            for (unsigned eraseItem : eraseSet)
              itempAssign.erase(eraseItem);
          }
        }
      }
      left = right;
    }
    standardize(irs);
  }
}

void IROptimizer::constPassBlock() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    vector<IR *> &irs = it->second;
    unsigned left = 0, right = 0;
    while (right < irs.size()) {
      while (right < irs.size()) {
        if (irs[right]->type == IR::BEQ || irs[right]->type == IR::BGE ||
            irs[right]->type == IR::BGT || irs[right]->type == IR::BLE ||
            irs[right]->type == IR::BLT || irs[right]->type == IR::BNE ||
            irs[right]->type == IR::CALL || irs[right]->type == IR::GOTO ||
            irs[right]->type == IR::LABEL)
          break;
        right++;
      }
      if (right < irs.size())
        right++;
      unordered_map<unsigned, unsigned> temp2Int;
      unordered_map<unsigned, unsigned> temp2Float;
      for (unsigned i = left; i < right; i++) {
        for (unsigned j = 1; j < irs[i]->items.size(); j++) {
          if (irs[i]->items[j]->type == IRItem::FTEMP &&
              temp2Float.find(irs[i]->items[j]->iVal) != temp2Float.end()) {
            irs[i]->items[j]->type = IRItem::FLOAT;
            irs[i]->items[j]->iVal = temp2Float[irs[i]->items[j]->iVal];
          } else if (irs[i]->items[j]->type == IRItem::ITEMP &&
                     temp2Int.find(irs[i]->items[j]->iVal) != temp2Int.end()) {
            irs[i]->items[j]->type = IRItem::INT;
            irs[i]->items[j]->iVal = temp2Int[irs[i]->items[j]->iVal];
          }
        }
        if (irs[i]->type == IR::MOV) {
          if (irs[i]->items[0]->type == IRItem::FTEMP) {
            if (irs[i]->items[1]->type == IRItem::FLOAT)
              temp2Float[irs[i]->items[0]->iVal] = irs[i]->items[1]->iVal;
            else
              temp2Float.erase(irs[i]->items[0]->iVal);
          } else if (irs[i]->items[0]->type == IRItem::ITEMP) {
            if (irs[i]->items[1]->type == IRItem::INT)
              temp2Int[irs[i]->items[0]->iVal] = irs[i]->items[1]->iVal;
            else
              temp2Int.erase(irs[i]->items[0]->iVal);
          }
        }
      }
      left = right;
    }
    standardize(irs);
  }
}

void IROptimizer::constPassGlobal() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    vector<IR *> &irs = it->second;
    unordered_map<unsigned, unsigned> temp2Int;
    unordered_map<unsigned, unsigned> temp2Float;
    unordered_map<unsigned, unsigned> defCounter;
    for (IR *ir : irs) {
      if (ir->type == IR::MOV) {
        if (ir->items[0]->type == IRItem::FTEMP &&
            ir->items[1]->type == IRItem::FLOAT) {
          temp2Float[ir->items[0]->iVal] = ir->items[1]->iVal;
        } else if (ir->items[0]->type == IRItem::ITEMP &&
                   ir->items[1]->type == IRItem::INT) {
          temp2Int[ir->items[0]->iVal] = ir->items[1]->iVal;
          defCounter[ir->items[0]->iVal]++;
        }
      }
      if (!ir->items.empty() && (ir->items[0]->type == IRItem::FTEMP ||
                                 ir->items[0]->type == IRItem::ITEMP))
        defCounter[ir->items[0]->iVal]++;
    }
    for (unordered_map<unsigned, unsigned>::iterator innerIt =
             defCounter.begin();
         innerIt != defCounter.end(); innerIt++)
      if (innerIt->second > 1) {
        temp2Float.erase(innerIt->first);
        temp2Int.erase(innerIt->first);
      }
    for (IR *ir : irs) {
      for (unsigned j = 1; j < ir->items.size(); j++) {
        if (ir->items[j]->type == IRItem::FTEMP &&
            temp2Float.find(ir->items[j]->iVal) != temp2Float.end()) {
          ir->items[j]->type = IRItem::FLOAT;
          ir->items[j]->iVal = temp2Float[ir->items[j]->iVal];
        } else if (ir->items[j]->type == IRItem::ITEMP &&
                   temp2Int.find(ir->items[j]->iVal) != temp2Int.end()) {
          ir->items[j]->type = IRItem::INT;
          ir->items[j]->iVal = temp2Int[ir->items[j]->iVal];
        }
      }
    }
    standardize(irs);
  }
}

void IROptimizer::deadCodeElimination() {
  unordered_set<Symbol *> todoFuncs;
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++)
    if (!it->first->name.compare("main")) {
      todoFuncs.insert(it->first);
      break;
    }
  unordered_set<unsigned> temps;
  unordered_set<Symbol *> symbols;
  unordered_map<Symbol *, unordered_set<unsigned>> symbol2Temp;
  unordered_map<unsigned, unordered_set<Symbol *>> temp2Symbol;
  unordered_map<unsigned, unordered_set<unsigned>> temp2Temp;
  unsigned originSize;
  do {
    originSize = todoFuncs.size() + temps.size() + symbols.size();
    for (Symbol *func : todoFuncs) {
      if (funcIRs.find(func) == funcIRs.end())
        continue;
      for (Symbol *param : func->params)
        if (!param->dimensions.empty())
          symbols.insert(param);
      for (IR *ir : funcIRs[func]) {
        if (ir->type == IR::BEQ || ir->type == IR::BGE || ir->type == IR::BGT ||
            ir->type == IR::BLE || ir->type == IR::BLT || ir->type == IR::BNE) {
          if (ir->items[1]->type == IRItem::FTEMP ||
              ir->items[1]->type == IRItem::ITEMP)
            temps.insert(ir->items[1]->iVal);
          if (ir->items[2]->type == IRItem::FTEMP ||
              ir->items[2]->type == IRItem::ITEMP)
            temps.insert(ir->items[2]->iVal);
        } else if (ir->type == IR::CALL) {
          todoFuncs.insert(ir->items[0]->symbol);
          for (unsigned i = 1; i < ir->items.size(); i++)
            if (ir->items[i]->type == IRItem::FTEMP ||
                ir->items[i]->type == IRItem::ITEMP)
              temps.insert(ir->items[i]->iVal);
        } else if (ir->type == IR::MOV &&
                   ir->items[0]->type == IRItem::RETURN) {
          if ((ir->items[1]->type == IRItem::FTEMP ||
               ir->items[1]->type == IRItem::ITEMP))
            temps.insert(ir->items[1]->iVal);
          else if (ir->items[1]->type == IRItem::SYMBOL) {
            symbols.insert(ir->items[1]->symbol);
            for (unsigned i = 2; i < ir->items.size(); i++)
              if ((ir->items[1]->type == IRItem::FTEMP ||
                   ir->items[1]->type == IRItem::ITEMP))
                temps.insert(ir->items[i]->iVal);
          }
        }
        switch (ir->type) {
        case IR::ADD:
        case IR::DIV:
        case IR::EQ:
        case IR::GE:
        case IR::GT:
        case IR::LE:
        case IR::LT:
        case IR::MOD:
        case IR::MUL:
        case IR::NE:
        case IR::SUB:
          if (ir->items[1]->type == IRItem::FTEMP ||
              ir->items[1]->type == IRItem::ITEMP)
            temp2Temp[ir->items[0]->iVal].insert(ir->items[1]->iVal);
          if (ir->items[2]->type == IRItem::FTEMP ||
              ir->items[2]->type == IRItem::ITEMP)
            temp2Temp[ir->items[0]->iVal].insert(ir->items[2]->iVal);
          break;
        case IR::F2I:
        case IR::I2F:
        case IR::L_NOT:
        case IR::NEG:
          temp2Temp[ir->items[0]->iVal].insert(ir->items[1]->iVal);
          break;
        case IR::MOV:
          if (ir->items[0]->type == IRItem::FTEMP ||
              ir->items[0]->type == IRItem::ITEMP) {
            for (unsigned i = 1; i < ir->items.size(); i++) {
              if (ir->items[i]->type == IRItem::FTEMP ||
                  ir->items[i]->type == IRItem::ITEMP)
                temp2Temp[ir->items[0]->iVal].insert(ir->items[i]->iVal);
              else if (ir->items[i]->type == IRItem::SYMBOL)
                temp2Symbol[ir->items[0]->iVal].insert(ir->items[i]->symbol);
            }
          } else if (ir->items[0]->type == IRItem::SYMBOL) {
            for (unsigned i = 1; i < ir->items.size(); i++)
              if (ir->items[i]->type == IRItem::FTEMP ||
                  ir->items[i]->type == IRItem::ITEMP)
                symbol2Temp[ir->items[0]->symbol].insert(ir->items[i]->iVal);
          }
          break;
        default:
          break;
        }
      }
      queue<unsigned> toAddTemps;
      for (unsigned temp : temps)
        toAddTemps.push(temp);
      queue<Symbol *> toAddSymbols;
      for (Symbol *symbol : symbols)
        toAddSymbols.push(symbol);
      while (!toAddSymbols.empty() || !toAddTemps.empty()) {
        while (!toAddSymbols.empty()) {
          Symbol *symbol = toAddSymbols.front();
          toAddSymbols.pop();
          if (symbol2Temp.find(symbol) != symbol2Temp.end())
            for (unsigned newTemp : symbol2Temp[symbol])
              if (temps.find(newTemp) == temps.end()) {
                temps.insert(newTemp);
                toAddTemps.push(newTemp);
              }
        }
        while (!toAddTemps.empty()) {
          unsigned temp = toAddTemps.front();
          toAddTemps.pop();
          if (temp2Symbol.find(temp) != temp2Symbol.end())
            for (Symbol *newSymbol : temp2Symbol[temp])
              if (symbols.find(newSymbol) == symbols.end()) {
                symbols.insert(newSymbol);
                toAddSymbols.push(newSymbol);
              }
          if (temp2Temp.find(temp) != temp2Temp.end())
            for (unsigned newTemp : temp2Temp[temp])
              if (temps.find(newTemp) == temps.end()) {
                temps.insert(newTemp);
                toAddTemps.push(newTemp);
              }
        }
      }
    }
  } while (originSize < todoFuncs.size() + temps.size() + symbols.size());
  unordered_map<Symbol *, vector<IR *>> newFuncs;
  for (Symbol *func : todoFuncs) {
    if (funcIRs.find(func) == funcIRs.end())
      continue;
    vector<IR *> newIRs;
    for (IR *ir : funcIRs[func]) {
      if (ir->type == IR::CALL || ir->type == IR::GOTO || ir->type == IR::LABEL)
        newIRs.push_back(ir);
      else {
        bool flag = true;
        for (IRItem *item : ir->items)
          if (((item->type == IRItem::FTEMP || item->type == IRItem::ITEMP) &&
               temps.find(item->iVal) == temps.end()) ||
              (item->type == IRItem::SYMBOL &&
               symbols.find(item->symbol) == symbols.end())) {
            flag = false;
            break;
          }
        if (flag)
          newIRs.push_back(ir);
      }
    }
    newFuncs[func] = newIRs;
  }
  funcIRs = newFuncs;
  vector<Symbol *> newConsts;
  for (Symbol *symbol : consts)
    if (symbols.find(symbol) != symbols.end())
      newConsts.push_back(symbol);
  consts = newConsts;
  vector<Symbol *> newGlobalVars;
  for (Symbol *symbol : globalVars)
    if (symbols.find(symbol) != symbols.end())
      newGlobalVars.push_back(symbol);
  globalVars = newGlobalVars;
  for (unordered_map<Symbol *, vector<Symbol *>>::iterator it =
           localVars.begin();
       it != localVars.end(); it++) {
    vector<Symbol *> newLocalVars;
    for (Symbol *symbol : it->second)
      if (symbols.find(symbol) != symbols.end())
        newLocalVars.push_back(symbol);
    it->second = newLocalVars;
  }
}

void IROptimizer::optimize() {
  singleVar2Reg();
  deadCodeElimination();
  constPassGlobal();
  constPassBlock();
  deadCodeElimination();
  assignPass();
  deadCodeElimination();
  splitTemps();
  deadCodeElimination();
  unsigned originSize;
  unsigned processedSize = 0;
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++)
    processedSize += it->second.size();
  do {
    originSize = processedSize;
    optimizeFlow();
    deadCodeElimination();
    constPassGlobal();
    constPassBlock();
    deadCodeElimination();
    processedSize = 0;
    for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
         it != funcIRs.end(); it++)
      processedSize += it->second.size();
  } while (originSize != processedSize);
}

void IROptimizer::optimizeFlow() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    bool toContinue;
    do {
      toContinue = false;
      vector<IR *> &irs = it->second;
      unordered_map<IR *, unsigned> irIdMap;
      for (unsigned i = 0; i < irs.size(); i++)
        irIdMap[irs[i]] = i;
      unordered_set<unsigned> usedLabelId;
      for (IR *ir : irs) {
        if (ir->type == IR::BEQ || ir->type == IR::BGE || ir->type == IR::BGT ||
            ir->type == IR::BLE || ir->type == IR::BLT || ir->type == IR::BNE ||
            ir->type == IR::GOTO) {
          unsigned labelId = irIdMap[ir->items[0]->ir];
          while (labelId + 1 < irs.size() &&
                 irs[labelId + 1]->type == IR::LABEL)
            labelId++;
          usedLabelId.insert(labelId);
          ir->items[0]->ir = irs[labelId];
        }
      }
      vector<IR *> newIRs;
      for (unsigned i = 0; i < irs.size(); i++)
        if (irs[i]->type != IR::LABEL ||
            usedLabelId.find(i) != usedLabelId.end())
          newIRs.push_back(irs[i]);
      if (irs.size() != newIRs.size())
        toContinue = true;
      irs = newIRs;
      newIRs.clear();
      for (unsigned i = 0; i < irs.size(); i++)
        irIdMap[irs[i]] = i;
      queue<unsigned> frontier;
      unsigned beginId = 0;
      while (irs[beginId]->type == IR::GOTO)
        beginId = irIdMap[irs[beginId]->items[0]->ir];
      frontier.push(beginId);
      unordered_set<unsigned> visited;
      while (!frontier.empty()) {
        unsigned curId = frontier.front();
        frontier.pop();
        if (visited.find(curId) != visited.end())
          continue;
        visited.insert(curId);
        if (irs[curId]->type == IR::BEQ || irs[curId]->type == IR::BGE ||
            irs[curId]->type == IR::BGT || irs[curId]->type == IR::BLE ||
            irs[curId]->type == IR::BLT || irs[curId]->type == IR::BNE) {
          if (curId + 1 < irs.size())
            frontier.push(curId + 1);
          frontier.push(irIdMap[irs[curId]->items[0]->ir]);
        } else if (irs[curId]->type == IR::GOTO)
          frontier.push(irIdMap[irs[curId]->items[0]->ir]);
        else {
          if (curId + 1 < irs.size())
            frontier.push(curId + 1);
        }
      }
      for (unsigned i = 0; i < irs.size(); i++)
        if (visited.find(i) != visited.end())
          newIRs.push_back(irs[i]);
      if (irs.size() != newIRs.size())
        toContinue = true;
      irs = newIRs;
      newIRs.clear();
      irIdMap.clear();
      for (unsigned i = 0; i < irs.size(); i++)
        irIdMap[irs[i]] = i;
      for (unsigned i = 0; i < irs.size(); i++) {
        if ((irs[i]->type == IR::BEQ || irs[i]->type == IR::BGE ||
             irs[i]->type == IR::BGT || irs[i]->type == IR::BLE ||
             irs[i]->type == IR::BLT || irs[i]->type == IR::BNE ||
             irs[i]->type == IR::GOTO) &&
            irIdMap[irs[i]->items[0]->ir] == i + 1)
          continue;
        newIRs.push_back(irs[i]);
      }
      if (irs.size() != newIRs.size())
        toContinue = true;
      irs = newIRs;
    } while (toContinue);
  }
}

void IROptimizer::singleVar2Reg() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    Symbol *func = it->first;
    vector<IR *> &irs = it->second;
    vector<IR *> newIRs;
    unordered_map<Symbol *, unsigned> symbol2tempId;
    for (Symbol *param : func->params) {
      if (param->dimensions.empty()) {
        symbol2tempId[param] = tempId++;
        newIRs.push_back(new IR(
            IR::MOV, {new IRItem(param->dataType == Symbol::INT ? IRItem::ITEMP
                                                                : IRItem::FTEMP,
                                 symbol2tempId[param]),
                      new IRItem(param)}));
      }
    }
    for (IR *ir : irs) {
      for (unsigned i = 0; i < ir->items.size(); i++) {
        if (ir->items[i]->symbol &&
            (ir->items[i]->symbol->symbolType == Symbol::LOCAL_VAR ||
             ir->items[i]->symbol->symbolType == Symbol::PARAM) &&
            ir->items[i]->symbol->dimensions.empty()) {
          if (symbol2tempId.find(ir->items[i]->symbol) == symbol2tempId.end())
            symbol2tempId[ir->items[i]->symbol] = tempId++;
          Symbol *symbol = ir->items[i]->symbol;
          ir->items[i] = new IRItem(
              symbol->dataType == Symbol::INT ? IRItem::ITEMP : IRItem::FTEMP,
              symbol2tempId[symbol]);
        }
      }
      newIRs.push_back(ir);
    }
    irs = newIRs;
  }
}

void IROptimizer::splitTemps() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    vector<IR *> &irs = it->second;
    unsigned tempNum = 0;
    unordered_map<unsigned, unsigned> reassignMap;
    for (IR *ir : irs) {
      for (IRItem *item : ir->items) {
        if (item->type != IRItem::FTEMP && item->type != IRItem::ITEMP)
          continue;
        if (reassignMap.find(item->iVal) != reassignMap.end())
          continue;
        reassignMap[item->iVal] = tempNum++;
      }
    }
    unordered_map<IR *, unsigned> irIdMap;
    unsigned irNum = 0;
    for (IR *ir : irs) {
      irIdMap[ir] = irNum;
      ir->irId = irNum++;
      for (IRItem *item : ir->items) {
        if (item->type != IRItem::FTEMP && item->type != IRItem::ITEMP)
          continue;
        item->iVal = reassignMap[item->iVal];
      }
    }
    vector<vector<unsigned>> nextMap(irNum);
    vector<set<unsigned>> rMap(tempNum);
    vector<set<unsigned>> wMap(tempNum);
    for (unsigned i = 0; i < irs.size(); i++) {
      if (irs[i]->type == IR::GOTO)
        nextMap[i].push_back(irIdMap[irs[i]->items[0]->ir]);
      else if (irs[i]->type == IR::BEQ || irs[i]->type == IR::BGE ||
               irs[i]->type == IR::BGT || irs[i]->type == IR::BLE ||
               irs[i]->type == IR::BLT || irs[i]->type == IR::BNE) {
        nextMap[i].push_back(i + 1);
        nextMap[i].push_back(irIdMap[irs[i]->items[0]->ir]);
      } else if (i + 1 < irs.size())
        nextMap[i].push_back(i + 1);
      for (unsigned j = 0; j < irs[i]->items.size(); j++) {
        if (irs[i]->items[j]->type == IRItem::FTEMP ||
            irs[i]->items[j]->type == IRItem::ITEMP) {
          if (j)
            rMap[irs[i]->items[j]->iVal].insert(i);
          else
            wMap[irs[i]->items[j]->iVal].insert(i);
        }
      }
    }
    for (unsigned temp = 0; temp < tempNum; temp++) {
      unordered_map<unsigned, unordered_set<unsigned>> w2RMap;
      for (unsigned source : wMap[temp]) {
        unordered_set<unsigned> visited;
        queue<unsigned> frontier;
        frontier.push(source);
        while (!frontier.empty()) {
          unsigned cur = frontier.front();
          frontier.pop();
          for (unsigned next : nextMap[cur]) {
            if (visited.find(next) != visited.end())
              continue;
            visited.insert(next);
            if (wMap[temp].find(next) == wMap[temp].end())
              frontier.push(next);
            if (rMap[temp].find(next) != rMap[temp].end())
              w2RMap[source].insert(next);
          }
        }
      }
      vector<unsigned> rIds(rMap[temp].begin(), rMap[temp].end());
      vector<unsigned> unionSet(rMap[temp].size());
      unordered_map<unsigned, unsigned> rIdMap;
      for (unsigned i = 0; i < rIds.size(); i++) {
        unionSet[i] = i;
        rIdMap[rIds[i]] = i;
      }
      vector<pair<unordered_set<unsigned>, unordered_set<unsigned>>> w2R;
      for (unordered_map<unsigned, unordered_set<unsigned>>::iterator innerIt =
               w2RMap.begin();
           innerIt != w2RMap.end(); innerIt++) {
        w2R.emplace_back(unordered_set({innerIt->first}), innerIt->second);
      }
      unsigned w2RSize;
      do {
        w2RSize = w2R.size();
        for (unsigned i = 0; i < w2R.size(); i++)
          for (unsigned j = i + 1; j < w2R.size(); j++) {
            unordered_set<unsigned> unionSet;
            unionSet.insert(w2R[i].second.begin(), w2R[i].second.end());
            unionSet.insert(w2R[j].second.begin(), w2R[j].second.end());
            if (w2R[i].second.size() + w2R[j].second.size() > unionSet.size()) {
              w2R[i].first.insert(w2R[j].first.begin(), w2R[j].first.end());
              w2R[i].second.insert(w2R[j].second.begin(), w2R[j].second.end());
              w2R.erase(w2R.begin() + j);
              j--;
            }
          }
      } while (w2R.size() < w2RSize);
      for (pair<unordered_set<unsigned>, unordered_set<unsigned>> &w2RItem :
           w2R) {
        for (unsigned wId : w2RItem.first)
          irs[wId]->items[0]->iVal = tempId;
        for (unsigned rId : w2RItem.second)
          for (IRItem *item : irs[rId]->items)
            if ((item->type == IRItem::FTEMP || item->type == IRItem::ITEMP) &&
                (unsigned)item->iVal == temp)
              item->iVal = tempId;
        tempId++;
      }
    }
  }
  unsigned id = 0;
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    vector<IR *> &irs = it->second;
    unordered_map<unsigned, unsigned> reassignMap;
    for (IR *ir : irs) {
      for (IRItem *item : ir->items) {
        if (item->type != IRItem::FTEMP && item->type != IRItem::ITEMP)
          continue;
        if (reassignMap.find(item->iVal) != reassignMap.end())
          continue;
        reassignMap[item->iVal] = id++;
      }
    }
    for (IR *ir : irs) {
      for (IRItem *item : ir->items) {
        if (item->type != IRItem::FTEMP && item->type != IRItem::ITEMP)
          continue;
        item->iVal = reassignMap[item->iVal];
      }
    }
  }
}

void IROptimizer::standardize(vector<IR *> &irs) {
  vector<IR *> newIRs;
  for (IR *ir : irs) {
    switch (ir->type) {
    case IR::ADD:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->fVal = ir->items[1]->fVal + ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal + ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::BEQ:
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal == ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal == ir->items[2]->iVal)) {
        ir->type = IR::GOTO;
        ir->items.resize(1);
        newIRs.push_back(ir);
        continue;
      }
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal != ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal != ir->items[2]->iVal))
        continue;
      newIRs.push_back(ir);
      break;
    case IR::BGE:
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal >= ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal >= ir->items[2]->iVal)) {
        ir->type = IR::GOTO;
        ir->items.resize(1);
        newIRs.push_back(ir);
        continue;
      }
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal < ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal < ir->items[2]->iVal))
        continue;
      newIRs.push_back(ir);
      break;
    case IR::BGT:
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal > ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal > ir->items[2]->iVal)) {
        ir->type = IR::GOTO;
        ir->items.resize(1);
        newIRs.push_back(ir);
        continue;
      }
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal <= ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal <= ir->items[2]->iVal))
        continue;
      newIRs.push_back(ir);
      break;
    case IR::BLE:
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal <= ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal <= ir->items[2]->iVal)) {
        ir->type = IR::GOTO;
        ir->items.resize(1);
        newIRs.push_back(ir);
        continue;
      }
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal > ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal > ir->items[2]->iVal))
        continue;
      newIRs.push_back(ir);
      break;
    case IR::BLT:
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal < ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal < ir->items[2]->iVal)) {
        ir->type = IR::GOTO;
        ir->items.resize(1);
        newIRs.push_back(ir);
        continue;
      }
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal >= ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal >= ir->items[2]->iVal))
        continue;
      newIRs.push_back(ir);
      break;
    case IR::BNE:
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal != ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal != ir->items[2]->iVal)) {
        ir->type = IR::GOTO;
        ir->items.resize(1);
        newIRs.push_back(ir);
        continue;
      }
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal == ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal == ir->items[2]->iVal))
        continue;
      newIRs.push_back(ir);
      break;
    case IR::DIV:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->fVal = ir->items[1]->fVal / ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal / ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::EQ:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->fVal = ir->items[1]->fVal == ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal == ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::F2I:
      if (ir->items[1]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->iVal = ir->items[1]->fVal;
      }
      newIRs.push_back(ir);
      break;
    case IR::GE:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->fVal = ir->items[1]->fVal >= ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal >= ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::GT:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->fVal = ir->items[1]->fVal > ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal > ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::I2F:
      if (ir->items[1]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::FLOAT;
        ir->items[1]->fVal = ir->items[1]->iVal;
      }
      newIRs.push_back(ir);
      break;
    case IR::L_NOT:
      if (ir->items[1]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->iVal = !ir->items[1]->fVal;
      } else if (ir->items[1]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = !ir->items[1]->iVal;
      }
      newIRs.push_back(ir);
      break;
    case IR::LE:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->fVal = ir->items[1]->fVal <= ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal <= ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::LT:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->fVal = ir->items[1]->fVal < ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal < ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::MOD:
      if (ir->items[1]->type == IRItem::INT &&
          ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal % ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::MUL:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->fVal = ir->items[1]->fVal * ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal * ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::NE:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->fVal = ir->items[1]->fVal != ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal != ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::NEG:
      if (ir->items[1]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = -ir->items[1]->fVal;
      } else if (ir->items[1]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = -ir->items[1]->iVal;
      }
      newIRs.push_back(ir);
      break;
    case IR::SUB:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->fVal = ir->items[1]->fVal - ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal - ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    default:
      newIRs.push_back(ir);
      break;
    }
  }
  irs = newIRs;
}