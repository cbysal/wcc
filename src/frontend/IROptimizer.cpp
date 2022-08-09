#include <queue>
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

void IROptimizer::optimize() {
  isProcessed = true;
  removeDeadCode();
  singleVar2Reg();
  removeDeadCode();
  simplePass();
  removeDuplicatedJumps();
  removeDuplicatedLabels();
  removeDuplicatedSymbols();
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
  unordered_map<Symbol *, vector<Symbol *>> newLocalVars;
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
    vector<Symbol *> newLocals;
    for (Symbol *symbol : symbols)
      if (symbol->symbolType == Symbol::LOCAL_VAR)
        newLocals.push_back(symbol);
    newLocalVars[func] = newLocals;
  }
  vector<Symbol *> newConsts;
  vector<Symbol *> newGlobalVars;
  for (Symbol *symbol : symbols) {
    switch (symbol->symbolType) {
    case Symbol::CONST:
      newConsts.push_back(symbol);
      break;
    case Symbol::GLOBAL_VAR:
      globalVars.push_back(symbol);
      break;
    default:
      break;
    }
  }
  funcs = newFuncs;
  consts = newConsts;
  globalVars = newGlobalVars;
  localVars = newLocalVars;
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

void IROptimizer::simplePass() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcs.begin();
       it != funcs.end(); it++) {
    vector<IR *> &irs = it->second;
    unsigned left = 0, right = 0;
    vector<IR *> newIRs;
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
      unordered_map<unsigned, unsigned> passMap;
      unordered_map<unsigned, unsigned> defCounter;
      for (unsigned i = left; i <= right; i++) {
        if (irs[i]->items.size() == 2 &&
            irs[i]->items[0]->type == IRItem::ITEMP &&
            irs[i]->items[1]->type == IRItem::INT) {
          defCounter[irs[i]->items[0]->iVal]++;
          passMap[irs[i]->items[0]->iVal] = irs[i]->items[1]->iVal;
        }
      }
      unordered_map<unsigned, unsigned> temp2Int;
      for (pair<unsigned, unsigned> pass : passMap)
        if (defCounter[pass.first] == 1)
          temp2Int.insert(pass);
      for (unsigned i = left; i <= right; i++) {
        if (!irs[i]->items.empty() && irs[i]->items[0]->type == IRItem::ITEMP &&
            temp2Int.find(irs[i]->items[0]->iVal) != temp2Int.end())
          continue;
        for (IRItem *item : irs[i]->items)
          if (item->type == IRItem::ITEMP &&
              temp2Int.find(item->iVal) != temp2Int.end()) {
            item->type = IRItem::INT;
            item->iVal = temp2Int[item->iVal];
          }
        newIRs.push_back(irs[i]);
      }
      right++;
      left = right;
    }
    irs = newIRs;
  }
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