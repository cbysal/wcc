#include <iostream>
#include <unordered_set>

#include "IROptimizer.h"

using namespace std;

IROptimizer::IROptimizer(
    const vector<Symbol *> &symbols, const vector<Symbol *> &consts,
    const vector<Symbol *> &globalVars,
    const unordered_map<Symbol *, vector<Symbol *>> &localVars,
    const vector<pair<Symbol *, vector<IR *>>> &funcs, unsigned tempId) {
  this->isProcessed = false;
  this->symbols = symbols;
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

void IROptimizer::deleteDeadCode() {
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
      for (IR *ir : func.second) {
        for (IRItem *item : ir->items) {
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

vector<pair<Symbol *, vector<IR *>>> IROptimizer::getFuncs() {
  if (!isProcessed)
    optimize();
  return funcs;
}

void IROptimizer::optimize() {
  isProcessed = true;
  deleteDeadCode();
  singleVar2Reg();
  deleteDeadCode();
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
