#include <algorithm>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <unordered_set>

#include "IROptimizer.h"

using namespace std;

IROptimizer::IROptimizer(
    const vector<Symbol *> &consts, const vector<Symbol *> &globalVars,
    const unordered_map<Symbol *, vector<Symbol *>> &localVars,
    const unordered_map<Symbol *, vector<IR *>> &funcs, unsigned tempId) {
  this->isProcessed = false;
  this->consts = consts;
  this->globalVars = globalVars;
  this->localVars = localVars;
  this->funcs = funcs;
  this->tempId = tempId;
}

IROptimizer::~IROptimizer() {
  for (IR *ir : toRecycleIRs)
    delete ir;
}

vector<Symbol *> IROptimizer::getConsts() {
  if (!isProcessed)
    optimize();
  return consts;
}

unordered_map<Symbol *, vector<IR *>> IROptimizer::getFuncs() {
  if (!isProcessed)
    optimize();
  return funcs;
}

vector<Symbol *> IROptimizer::getGlobalVars() {
  if (!isProcessed)
    optimize();
  return globalVars;
}

unordered_map<Symbol *, vector<Symbol *>> IROptimizer::getLocalVars() {
  if (!isProcessed)
    optimize();
  return localVars;
}

unsigned IROptimizer::getTempId() { return tempId; }

void IROptimizer::constPassBlock() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcs.begin();
       it != funcs.end(); it++) {
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
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcs.begin();
       it != funcs.end(); it++) {
    vector<IR *> &irs = it->second;
    unordered_map<unsigned, unsigned> temp2Int;
    unordered_map<unsigned, unsigned> temp2Float;
    unordered_set<unsigned> multiDef;
    for (IR *ir : irs) {
      if (ir->type == IR::MOV) {
        if (ir->items[0]->type == IRItem::FTEMP &&
            ir->items[1]->type == IRItem::FLOAT) {
          if (multiDef.find(ir->items[0]->iVal) == multiDef.end())
            temp2Float[ir->items[0]->iVal] = ir->items[1]->iVal;
          else {
            multiDef.insert(ir->items[0]->iVal);
            temp2Float.erase(ir->items[0]->iVal);
          }
        } else if (ir->items[0]->type == IRItem::ITEMP &&
                   ir->items[1]->type == IRItem::INT) {
          if (multiDef.find(ir->items[0]->iVal) == multiDef.end())
            temp2Int[ir->items[0]->iVal] = ir->items[1]->iVal;
          else {
            multiDef.insert(ir->items[0]->iVal);
            temp2Int.erase(ir->items[0]->iVal);
          }
        }
      }
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

void IROptimizer::optimize() {
  isProcessed = true;
  singleVar2Reg();
  removeDeadCode();
  constPassGlobal();
  constPassBlock();
  removeDeadCode();
  // splitTempsNotStrict();
  // splitTemps();
  // removeDuplicatedJumps();
  // removeDuplicatedLabels();
  // removeDuplicatedSymbols();
}

void IROptimizer::removeDeadCode() {
  unordered_set<Symbol *> todoFuncs;
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcs.begin();
       it != funcs.end(); it++)
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
      if (funcs.find(func) == funcs.end())
        continue;
      for (Symbol *param : func->params)
        if (!param->dimensions.empty())
          symbols.insert(param);
      for (IR *ir : funcs[func]) {
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
    if (funcs.find(func) == funcs.end())
      continue;
    vector<IR *> newIRs;
    for (IR *ir : funcs[func]) {
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
  funcs = newFuncs;
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

void IROptimizer::removeDuplicatedJumps() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcs.begin();
       it != funcs.end(); it++) {
    vector<IR *> &irs = it->second;
    bool toContinue = false;
    do {
      toContinue = false;
      unordered_map<IR *, unsigned> labelIdMap;
      for (unsigned i = 0; i < irs.size(); i++)
        if (irs[i]->type == IR::LABEL)
          labelIdMap[irs[i]] = i;
      unordered_set<IR *> duplicatedJumps;
      for (unsigned i = 0; i < irs.size(); i++) {
        if ((irs[i]->type == IR::BEQ || irs[i]->type == IR::BGE ||
             irs[i]->type == IR::BGT || irs[i]->type == IR::BLE ||
             irs[i]->type == IR::BLT || irs[i]->type == IR::BNE ||
             irs[i]->type == IR::GOTO) &&
            labelIdMap[irs[i]->items[0]->ir] > i) {
          bool flag = true;
          for (unsigned j = i + 1; j < labelIdMap[irs[i]->items[0]->ir]; j++)
            if (irs[j]->type != IR::LABEL) {
              flag = false;
              break;
            }
          if (flag) {
            toContinue = true;
            duplicatedJumps.insert(irs[i]);
          }
        }
      }
      vector<IR *> newIRs;
      for (IR *ir : irs)
        if (duplicatedJumps.find(ir) == duplicatedJumps.end())
          newIRs.push_back(ir);
      irs = newIRs;
    } while (toContinue);
  }
}

void IROptimizer::removeDuplicatedLabels() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcs.begin();
       it != funcs.end(); it++) {
    vector<IR *> &irs = it->second;
    unordered_map<IR *, IR *> labelUnion;
    for (unsigned i = 0; i < irs.size(); i++) {
      if (i + 1 < irs.size() && irs[i]->type == IR::LABEL &&
          irs[i + 1]->type == IR::LABEL) {
        IR *label = irs[i];
        while (labelUnion.find(label) != labelUnion.end())
          label = labelUnion[label];
        labelUnion[irs[i + 1]] = label;
      }
    }
    vector<IR *> newIRs;
    for (IR *ir : irs) {
      if ((ir->type == IR::BEQ || ir->type == IR::BGE || ir->type == IR::BGT ||
           ir->type == IR::BLE || ir->type == IR::BLT || ir->type == IR::BNE ||
           ir->type == IR::GOTO) &&
          labelUnion.find(ir->items[0]->ir) != labelUnion.end())
        ir->items[0]->ir = labelUnion[ir->items[0]->ir];
      if (ir->type != IR::LABEL || labelUnion.find(ir) == labelUnion.end())
        newIRs.push_back(ir);
    }
    irs = newIRs;
  }
}

void IROptimizer::removeDuplicatedSymbols() {
  unordered_set<Symbol *> newConsts;
  unordered_set<Symbol *> newGlobalVars;
  unordered_map<Symbol *, unordered_set<Symbol *>> newLocalVars;
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcs.begin();
       it != funcs.end(); it++) {
    Symbol *func = it->first;
    vector<IR *> &irs = it->second;
    for (IR *ir : irs)
      for (IRItem *item : ir->items)
        if (item->symbol) {
          switch (item->symbol->symbolType) {
          case Symbol::CONST:
            newConsts.insert(item->symbol);
            break;
          case Symbol::GLOBAL_VAR:
            newGlobalVars.insert(item->symbol);
            break;
          case Symbol::LOCAL_VAR:
            newLocalVars[func].insert(item->symbol);
            break;
          default:
            break;
          }
        }
  }
  consts = vector<Symbol *>(newConsts.begin(), newConsts.end());
  globalVars = vector<Symbol *>(newGlobalVars.begin(), newGlobalVars.end());
  for (unordered_map<Symbol *, vector<Symbol *>>::iterator it =
           localVars.begin();
       it != localVars.end(); it++)
    it->second = vector<Symbol *>(newLocalVars[it->first].begin(),
                                  newLocalVars[it->first].end());
}

void IROptimizer::singleVar2Reg() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcs.begin();
       it != funcs.end(); it++) {
    Symbol *func = it->first;
    vector<IR *> &irs = it->second;
    vector<IR *> newIRs;
    unordered_map<Symbol *, unsigned> symbol2tempId;
    for (Symbol *param : func->params) {
      if (param->dimensions.empty()) {
        symbol2tempId[param] = tempId++;
        toRecycleIRs.push_back(new IR(
            IR::MOV, {new IRItem(param->dataType == Symbol::INT ? IRItem::ITEMP
                                                                : IRItem::FTEMP,
                                 symbol2tempId[param]),
                      new IRItem(param)}));
        newIRs.push_back(toRecycleIRs.back());
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
          delete ir->items[i];
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
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcs.begin();
       it != funcs.end(); it++) {
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
    vector<vector<unsigned>> prevMap(irNum);
    vector<set<unsigned>> rMap(tempNum);
    vector<set<unsigned>> wMap(tempNum);
    for (unsigned i = 0; i < irs.size(); i++) {
      if (irs[i]->type == IR::GOTO)
        prevMap[irIdMap[irs[i]->items[0]->ir]].push_back(i);
      else if (irs[i]->type == IR::BEQ || irs[i]->type == IR::BGE ||
               irs[i]->type == IR::BGT || irs[i]->type == IR::BLE ||
               irs[i]->type == IR::BLT || irs[i]->type == IR::BNE) {
        prevMap[i + 1].push_back(i);
        prevMap[irIdMap[irs[i]->items[0]->ir]].push_back(i);
      } else if (i + 1 < irs.size())
        prevMap[i + 1].push_back(i);
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
      rMap[temp].insert(irs.size() - 1);
      vector<pair<unsigned, unsigned>> eraseTargets;
      for (unsigned source : rMap[temp]) {
        unsigned minTarget = UINT32_MAX;
        unsigned maxTarget = 0;
        unordered_set<unsigned> visited;
        queue<unsigned> frontier;
        frontier.push(source);
        visited.insert(source);
        while (!frontier.empty()) {
          unsigned cur = frontier.front();
          frontier.pop();
          for (unsigned prev : prevMap[cur]) {
            if (visited.find(prev) != visited.end())
              continue;
            visited.insert(prev);
            if (wMap[temp].find(prev) == wMap[temp].end())
              frontier.push(prev);
            else {
              minTarget = min(minTarget, prev);
              maxTarget = max(maxTarget, prev);
            }
          }
        }
        if (minTarget < maxTarget && wMap[temp].upper_bound(minTarget) !=
                                         wMap[temp].upper_bound(maxTarget))
          eraseTargets.emplace_back(minTarget, maxTarget);
      }
      for (const pair<unsigned, unsigned> &eraseTarget : eraseTargets)
        wMap[temp].erase(wMap[temp].upper_bound(eraseTarget.first),
                         wMap[temp].upper_bound(eraseTarget.second));
      vector<unsigned> newIndexes(wMap[temp].begin(), wMap[temp].end());
      newIndexes.push_back(irs.size());
      for (unsigned i = 0; i < newIndexes.size() - 1; i++) {
        for (unsigned j = newIndexes[i]; j < newIndexes[i + 1]; j++)
          for (IRItem *item : irs[j]->items)
            if ((item->type == IRItem::FTEMP || item->type == IRItem::ITEMP) &&
                (unsigned)item->iVal == temp)
              item->iVal = tempId;
        tempId++;
      }
    }
  }
  unsigned id = 0;
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcs.begin();
       it != funcs.end(); it++) {
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

void IROptimizer::splitTempsNotStrict() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcs.begin();
       it != funcs.end(); it++) {
    vector<IR *> &irs = it->second;
    unsigned left = 0, right = 0;
    while (right < irs.size()) {
      while (right < irs.size()) {
        if (right + 1 < irs.size() && irs[right + 1]->type == IR::LABEL)
          break;
        if (irs[right]->type == IR::BEQ || irs[right]->type == IR::BGE ||
            irs[right]->type == IR::BGT || irs[right]->type == IR::BLE ||
            irs[right]->type == IR::BLT || irs[right]->type == IR::BNE ||
            irs[right]->type == IR::GOTO)
          break;
        right++;
      }
      if (right == irs.size())
        right--;
      unsigned lastWrite = right;
      while (lastWrite > left) {
        if (irs[lastWrite]->items.empty() ||
            (irs[lastWrite]->items[0]->type != IRItem::FTEMP &&
             irs[lastWrite]->items[0]->type != IRItem::ITEMP)) {
          lastWrite--;
          continue;
        }
        unsigned writeBegin = left;
        while (writeBegin < lastWrite) {
          if (!irs[writeBegin]->items.empty() &&
              (irs[writeBegin]->items[0]->type == IRItem::FTEMP ||
               irs[writeBegin]->items[0]->type == IRItem::ITEMP) &&
              irs[writeBegin]->items[0]->iVal ==
                  irs[lastWrite]->items[0]->iVal) {
            break;
          }
          writeBegin++;
        }
        unsigned writeEnd = writeBegin + 1;
        while (writeEnd < lastWrite) {
          while (writeEnd < lastWrite) {
            if (!irs[lastWrite]->items.empty() &&
                (irs[lastWrite]->items[0]->type == IRItem::FTEMP ||
                 irs[lastWrite]->items[0]->type == IRItem::ITEMP) &&
                irs[writeEnd]->items[0]->iVal ==
                    irs[lastWrite]->items[0]->iVal) {
              break;
            }
            writeEnd++;
          }
          irs[writeBegin]->items[0]->iVal = tempId;
          for (unsigned i = writeBegin + 1; i < writeEnd; i++)
            for (IRItem *item : irs[i]->items)
              if ((item->type == IRItem::FTEMP ||
                   item->type == IRItem::ITEMP) &&
                  item->iVal == irs[lastWrite]->items[0]->iVal)
                item->iVal = tempId;
          for (unsigned i = 1; i < irs[writeEnd]->items.size(); i++)
            if ((irs[writeEnd]->items[i]->type == IRItem::FTEMP ||
                 irs[writeEnd]->items[i]->type == IRItem::ITEMP) &&
                irs[writeEnd]->items[i]->iVal == irs[lastWrite]->items[0]->iVal)
              irs[writeEnd]->items[i]->iVal = tempId;
          writeBegin = writeEnd;
          writeEnd++;
        }
        lastWrite--;
      }
      right++;
      left = right;
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