#include <iostream>
#include <unordered_set>

#include "IROptimizer.h"

using namespace std;

IROptimizer::IROptimizer(
    const vector<Symbol *> &consts, const vector<Symbol *> &globalVars,
    const unordered_map<Symbol *, vector<Symbol *>> &localVars,
    const vector<pair<Symbol *, vector<IR *>>> &funcs, unsigned tempId) {
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

unordered_set<unsigned> IROptimizer::getBlockBegins(const vector<IR *> &irs) {
  unordered_map<IR *, unsigned> irIdMap;
  for (unsigned i = 0; i < irs.size(); i++)
    irIdMap[irs[i]] = i;
  unordered_set<unsigned> blockBegins;
  for (unsigned i = 0; i < irs.size(); i++) {
    if (irs[i]->type == IR::BEQ || irs[i]->type == IR::BGE ||
        irs[i]->type == IR::BGT || irs[i]->type == IR::BLE ||
        irs[i]->type == IR::BLT || irs[i]->type == IR::BNE ||
        irs[i]->type == IR::GOTO) {
      blockBegins.insert(i + 1);
      blockBegins.insert(irIdMap[irs[i]->items[0]->ir]);
    } else if (irs[i]->type == IR::CALL)
      blockBegins.insert(i + 1);
  }
  return blockBegins;
}

vector<Symbol *> IROptimizer::getConsts() {
  if (!isProcessed)
    optimize();
  return consts;
}

vector<pair<Symbol *, vector<IR *>>> IROptimizer::getFuncs() {
  if (!isProcessed)
    optimize();
  return funcs;
}

vector<Symbol *> IROptimizer::getGlobalVars() {
  if (!isProcessed)
    optimize();
  return globalVars;
}

void IROptimizer::optimize() {
  isProcessed = true;
  removeDeadCode();
  singleVar2Reg();
  removeDeadCode();
  removeDuplicatedJumps();
  removeDuplicatedLabels();
  removeDuplicatedSymbols();
}

void IROptimizer::printIRs() {
  if (!isProcessed)
    optimize();
  for (Symbol *cst : consts)
    cout << cst->toString() << endl;
  for (Symbol *cst : globalVars)
    cout << cst->toString() << endl;
  for (pair<Symbol *, vector<IR *>> func : funcs) {
    cout << func.first->name << endl;
    for (IR *ir : func.second)
      cout << ir->toString() << endl;
  }
}

void IROptimizer::removeDeadCode() {
  for (pair<Symbol *, vector<IR *>> &func : funcs) {
    unordered_set<int> usedTemps;
    unordered_set<Symbol *> usedSymbols;
    unordered_set<IR *> jumpIRs;
    for (Symbol *symbol : func.first->params)
      usedSymbols.insert(symbol);
    for (IR *ir : func.second) {
      if (ir->type == IR::BEQ || ir->type == IR::BGE || ir->type == IR::BGT ||
          ir->type == IR::BLE || ir->type == IR::BLT || ir->type == IR::BNE) {
        jumpIRs.insert(ir->items[0]->ir);
        if (ir->items[1]->type == IRItem::ITEMP ||
            ir->items[1]->type == IRItem::FTEMP)
          usedTemps.insert(ir->items[1]->iVal);
        if (ir->items[2]->type == IRItem::ITEMP ||
            ir->items[2]->type == IRItem::FTEMP)
          usedTemps.insert(ir->items[2]->iVal);
      }
      if (ir->type == IR::GOTO)
        jumpIRs.insert(ir->items[0]->ir);
      if (ir->type == IR::CALL)
        for (unsigned i = 1; i < ir->items.size(); i++)
          usedTemps.insert(ir->items[i]->iVal);
      if (ir->type == IR::MOV && ir->items[0]->type == IRItem::SYMBOL &&
          ir->items[0]->symbol->symbolType == Symbol::GLOBAL_VAR)
        usedSymbols.insert(ir->items[0]->symbol);
      if (ir->type == IR::MOV && ir->items[1]->type == IRItem::SYMBOL &&
          ir->items[1]->symbol->symbolType == Symbol::GLOBAL_VAR)
        usedSymbols.insert(ir->items[1]->symbol);
      if (ir->type == IR::RETURN && func.first->dataType != Symbol::VOID)
        usedTemps.insert(ir->items[0]->iVal);
    }
    unsigned size = 0;
    while (size != usedTemps.size() + usedSymbols.size()) {
      size = usedTemps.size() + usedSymbols.size();
      for (IR *ir : func.second)
        for (IRItem *item : ir->items)
          if ((item->type == IRItem::SYMBOL &&
               usedSymbols.find(item->symbol) != usedSymbols.end()) ||
              ((item->type == IRItem::ITEMP || item->type == IRItem::FTEMP) &&
               usedTemps.find(item->iVal) != usedTemps.end())) {
            for (IRItem *aItem : ir->items) {
              if (aItem->type == IRItem::SYMBOL)
                usedSymbols.insert(aItem->symbol);
              if (aItem->type == IRItem::ITEMP || aItem->type == IRItem::FTEMP)
                usedTemps.insert(aItem->iVal);
            }
            break;
          }
    }
    vector<IR *> newIRs;
    for (IR *ir : func.second) {
      if (jumpIRs.find(ir) != jumpIRs.end() || ir->type == IR::BEQ ||
          ir->type == IR::BGE || ir->type == IR::BGT || ir->type == IR::BLE ||
          ir->type == IR::BLT || ir->type == IR::BNE || ir->type == IR::CALL ||
          ir->type == IR::GOTO || ir->type == IR::LABEL ||
          ir->type == IR::MEMSET_ZERO || ir->type == IR::RETURN) {
        newIRs.push_back(ir);
        continue;
      }
      bool flag = false;
      for (IRItem *item : ir->items)
        if ((item->type == IRItem::SYMBOL &&
             usedSymbols.find(item->symbol) != usedSymbols.end()) ||
            ((item->type == IRItem::ITEMP || item->type == IRItem::FTEMP) &&
             usedTemps.find(item->iVal) != usedTemps.end())) {
          flag = true;
          break;
        }
      if (flag)
        newIRs.push_back(ir);
      else
        jumpIRs.find(ir);
    }
    func.second = newIRs;
  }
}

void IROptimizer::removeDuplicatedJumps() {
  for (pair<Symbol *, vector<IR *>> &func : funcs) {
    bool toContinue = false;
    do {
      toContinue = false;
      unordered_map<IR *, unsigned> labelIdMap;
      for (unsigned i = 0; i < func.second.size(); i++)
        if (func.second[i]->type == IR::LABEL)
          labelIdMap[func.second[i]] = i;
      unordered_set<IR *> duplicatedJumps;
      for (unsigned i = 0; i < func.second.size(); i++) {
        if ((func.second[i]->type == IR::BEQ ||
             func.second[i]->type == IR::BGE ||
             func.second[i]->type == IR::BGT ||
             func.second[i]->type == IR::BLE ||
             func.second[i]->type == IR::BLT ||
             func.second[i]->type == IR::BNE ||
             func.second[i]->type == IR::GOTO) &&
            labelIdMap[func.second[i]->items[0]->ir] > i) {
          bool flag = true;
          for (unsigned j = i + 1; j < labelIdMap[func.second[i]->items[0]->ir];
               j++)
            if (func.second[j]->type != IR::LABEL) {
              flag = false;
              break;
            }
          if (flag) {
            toContinue = true;
            duplicatedJumps.insert(func.second[i]);
          }
        }
      }
      vector<IR *> newIRs;
      for (IR *ir : func.second)
        if (duplicatedJumps.find(ir) == duplicatedJumps.end())
          newIRs.push_back(ir);
      func.second = newIRs;
    } while (toContinue);
  }
}

void IROptimizer::removeDuplicatedLabels() {
  for (pair<Symbol *, vector<IR *>> &func : funcs) {
    unordered_map<IR *, IR *> labelUnion;
    for (unsigned i = 0; i < func.second.size(); i++) {
      if (i + 1 < func.second.size() && func.second[i]->type == IR::LABEL &&
          func.second[i + 1]->type == IR::LABEL) {
        IR *label = func.second[i];
        while (labelUnion.find(label) != labelUnion.end())
          label = labelUnion[label];
        labelUnion[func.second[i + 1]] = label;
      }
    }
    vector<IR *> newIRs;
    for (IR *ir : func.second) {
      if ((ir->type == IR::BEQ || ir->type == IR::BGE || ir->type == IR::BGT ||
           ir->type == IR::BLE || ir->type == IR::BLT || ir->type == IR::BNE ||
           ir->type == IR::GOTO) &&
          labelUnion.find(ir->items[0]->ir) != labelUnion.end())
        ir->items[0]->ir = labelUnion[ir->items[0]->ir];
      if (ir->type != IR::LABEL || labelUnion.find(ir) == labelUnion.end())
        newIRs.push_back(ir);
    }
    func.second = newIRs;
  }
}

void IROptimizer::removeDuplicatedSymbols() {
  unordered_set<Symbol *> newConsts;
  unordered_set<Symbol *> newGlobalVars;
  unordered_map<Symbol *, unordered_set<Symbol *>> newLocalVars;
  for (pair<Symbol *, vector<IR *>> &func : funcs)
    for (IR *ir : func.second)
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
            newLocalVars[func.first].insert(item->symbol);
            break;
          default:
            break;
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
  for (pair<Symbol *, vector<IR *>> &func : funcs) {
    vector<IR *> irs;
    unordered_map<Symbol *, unsigned> symbol2tempId;
    for (Symbol *param : func.first->params) {
      if (param->dimensions.empty()) {
        symbol2tempId[param] = tempId++;
        toRecycleIRs.push_back(new IR(
            IR::MOV, {new IRItem(param->dataType == Symbol::INT ? IRItem::ITEMP
                                                                : IRItem::FTEMP,
                                 symbol2tempId[param]),
                      new IRItem(param)}));
        irs.push_back(toRecycleIRs.back());
      }
    }
    for (IR *ir : func.second) {
      for (unsigned i = 0; i < ir->items.size(); i++) {
        if (ir->items[i]->symbol &&
            (ir->items[i]->symbol->symbolType == Symbol::LOCAL_VAR ||
             ir->items[i]->symbol->symbolType == Symbol::PARAM) &&
            ir->items[i]->symbol->dimensions.empty()) {
          if (symbol2tempId.find(ir->items[i]->symbol) == symbol2tempId.end())
            symbol2tempId[ir->items[i]->symbol] = tempId++;
          delete ir->items[i];
          ir->items[i] = new IRItem(
              ir->items[i]->symbol->dataType == Symbol::INT ? IRItem::ITEMP
                                                            : IRItem::FTEMP,
              symbol2tempId[ir->items[i]->symbol]);
        }
      }
      irs.push_back(ir);
    }
    func.second = irs;
  }
}