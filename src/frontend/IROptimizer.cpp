#include <functional>
#include <iostream>
#include <set>
#include <unordered_set>

// #define DEBUG

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
  // flowOptimize();
  // removeDeadCode();
  // singleVar2Reg();
  // removeDeadCode();
  // removeDuplicatedJumps();
  // removeDuplicatedLabels();
  // removeDuplicatedSymbols();
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

void IROptimizer::flowOptimize() {
  const auto branch =
      set<int>{IR::BEQ, IR::BGE, IR::BGT, IR::BGT, IR::BLE, IR::BLT, IR::BNE};

  auto Tarjan = [&](const vector<vector<int>> &G) {
    int n = G.size(), dft = 0, cnt = 0;
    vector<int> dfn(n), low(n), col(n), in(n), stk;
    function<void(int)> dfs = [&](int u) {
      dfn[u] = low[u] = ++dft;
      stk.push_back(u), in[u] = 1;
      for (auto v : G[u])
        if (!dfn[v])
          dfs(v), low[u] = min(low[u], low[v]);
        else if (in[v])
          low[u] = min(low[u], dfn[v]);
      if (dfn[u] == low[u]) {
        int v;
        ++cnt;
        do {
          v = stk.back();
          stk.pop_back();
          col[v] = cnt, in[v] = 0;
        } while (u != v);
      }
    };
    dfs(0);
    return col;
  };

  auto alu = [&](const auto &a, const auto &b, const IR::IRType &t) {
    switch (t) {
    case IR::ADD:
      return a + b;
    case IR::SUB:
      return a - b;
    case IR::MUL:
      return a * b;
    case IR::DIV:
      return a / b;
    case IR::MOD:
      return a - int(a / b) * b;
    default:
      break;
    }
    return 0 * a;
  };

  auto judge = [&](const auto &a, const auto &b, const IR::IRType &t) -> int {
    switch (t) {
    case IR::EQ:
    case IR::BEQ:
      return a == b;
    case IR::GE:
    case IR::BGE:
      return a >= b;
    case IR::GT:
    case IR::BGT:
      return a > b;
    case IR::LE:
    case IR::BLE:
      return a <= b;
    case IR::LT:
    case IR::BLT:
      return a < b;
    case IR::NE:
    case IR::BNE:
      return a != b;
    default:
      break;
    }
    return -1;
  };

  // global constant value table
  unordered_map<Symbol *, int> isym;
  unordered_map<Symbol *, float> fsym;

  // block constant propagation
  auto blockCP = [&](vector<IR *> &b, const set<Symbol *> &ban) {
    unordered_map<int, int> itmp;
    unordered_map<int, float> ftmp;
    vector<IR *> newb;
    for (auto &ir : b) {
      int push = 1;
      if (ir->type == IR::GOTO || ir->type == IR::LABEL) {
        newb.push_back(ir);
        continue;
      }
      // replace source operand constant
      for (size_t i = 1; i < ir->items.size(); ++i) {
        auto &item = ir->items[i];
        if (item->type == IRItem::ITEMP && itmp.count(item->iVal)) {
          item->type = IRItem::INT;
          item->iVal = itmp[item->iVal];
        } else if (item->type == IRItem::FTEMP && ftmp.count(item->iVal)) {
          item->type = IRItem::FLOAT;
          item->fVal = ftmp[item->iVal];
        } else if (item->type == IRItem::SYMBOL &&
                   item->symbol->dataType == Symbol::DataType::INT &&
                   isym.count(item->symbol)) {
          item->type = IRItem::INT;
          item->iVal = isym[item->symbol];
        } else if (item->type == IRItem::SYMBOL &&
                   item->symbol->dataType == Symbol::DataType::FLOAT &&
                   fsym.count(item->symbol)) {
          item->type = IRItem::FLOAT;
          item->fVal = fsym[item->symbol];
        }
      }
      auto t0 = ir->items[0]->type; // Destination operand type
      auto v0 = ir->items[0]->iVal; // Destination operand id
      // Remove unknown quantity from constant table
      int isConst = 1;
      for (size_t i = 1; i < ir->items.size(); ++i) {
        auto type = ir->items[i]->type;
        if (type == IRItem::ITEMP || type == IRItem::FTEMP ||
            type == IRItem::SYMBOL) {
          if (t0 == IRItem::ITEMP && itmp.count(v0)) {
            isConst = 0;
            itmp.erase(v0);
          } else if (t0 == IRItem::FTEMP && ftmp.count(v0)) {
            isConst = 0;
            ftmp.erase(v0);
          } else if (t0 == IRItem::SYMBOL) {
            auto dataType = ir->items[0]->symbol->dataType;
            auto symbol = ir->items[0]->symbol;
            if (dataType == Symbol::DataType::INT && isym.count(symbol)) {
              isConst = 0;
              isym.erase(symbol);
            } else if (dataType == Symbol::DataType::FLOAT &&
                       fsym.count(symbol)) {
              isConst = 0;
              fsym.erase(symbol);
            }
          }
        }
      }
      if (!isConst) {
        newb.push_back(ir);
        continue;
      }
      // constant propagation and constant calculation
      switch (ir->type) {
      case IR::MOV: {
        if (ir->items.size() > 2 || ir->items[1]->type == IRItem::RETURN)
          break;
        auto t1 = ir->items[1]->type;
        auto v1 = ir->items[1]->iVal;
        auto s0 = ir->items[0]->symbol;
        if (t1 == IRItem::INT) {
          if (t0 == IRItem::ITEMP) {
            push = 0;
            itmp[v0] = v1;
          } else if (s0 != NULL && !ban.count(s0) &&
                     s0->symbolType != Symbol::SymbolType::GLOBAL_VAR) {
            isym[s0] = v1;
          }
        } else if (t1 == IRItem::FLOAT) {
          if (t0 == IRItem::FTEMP) {
            push = 0;
            ftmp[v0] = ir->items[1]->fVal;
          } else if (s0 != NULL && !ban.count(s0) &&
                     s0->symbolType != Symbol::SymbolType::GLOBAL_VAR) {
            fsym[s0] = ir->items[1]->fVal;
          }
        }
        break;
      }
      case IR::ADD:
      case IR::SUB:
      case IR::MUL:
      case IR::DIV:
      case IR::MOD: {
        auto t1 = ir->items[1]->type;
        auto t2 = ir->items[2]->type;
        if (t1 == IRItem::INT && t2 == IRItem::INT) {
          push = 0;
          itmp[v0] = alu(ir->items[1]->iVal, ir->items[2]->iVal, ir->type);
        } else if (t1 == IRItem::FLOAT && t2 == IRItem::FLOAT) {
          push = 0;
          ftmp[v0] = alu(ir->items[1]->fVal, ir->items[2]->fVal, ir->type);
        }
        break;
      }
      case IR::EQ:
      case IR::GE:
      case IR::GT:
      case IR::LE:
      case IR::LT:
      case IR::NE: {
        auto t1 = ir->items[1]->type;
        auto t2 = ir->items[2]->type;
        if (t1 == IRItem::INT && t2 == IRItem::INT) {
          push = 0;
          itmp[v0] = judge(ir->items[1]->iVal, ir->items[2]->iVal, ir->type);
        } else if (t1 == IRItem::FLOAT && t2 == IRItem::FLOAT) {
          push = 0;
          ftmp[v0] = judge(ir->items[1]->fVal, ir->items[2]->fVal, ir->type);
        }
        break;
      }
      case IR::F2I:
        if (ir->items[1]->type == IRItem::FLOAT) {
          push = 0;
          itmp[v0] = int(ir->items[1]->fVal);
        }
        break;
      case IR::I2F:
        if (ir->items[1]->type == IRItem::INT) {
          push = 0;
          ftmp[v0] = float(ir->items[1]->iVal);
        }
        break;
      case IR::L_NOT:
        if (ir->items[1]->type == IRItem::INT) {
          push = 0;
          itmp[v0] = !ir->items[1]->iVal;
        } else if (ir->items[1]->type == IRItem::FLOAT) {
          push = 0;
          ftmp[v0] = !ir->items[1]->fVal;
        }
        break;
      case IR::NEG:
        if (ir->items[1]->type == IRItem::INT) {
          push = 0;
          itmp[v0] = -ir->items[1]->iVal;
        } else if (ir->items[1]->type == IRItem::FLOAT) {
          push = 0;
          ftmp[v0] = -ir->items[1]->fVal;
        }
        break;
      default:
        break;
      }
      if (push)
        newb.push_back(ir);
    }
    return newb;
  };

  for (auto &func : funcs) {
    int cnt = 0;
    vector<vector<IR *>> block(1);
    unordered_map<int, int> belong;
    // Partition program block
    for (auto &ir : func.second) {
      if (ir->type == IR::LABEL && !block[cnt].empty()) {
        ++cnt;
        block.push_back({});
      }
      block[cnt].push_back(ir);
      belong[ir->irId] = cnt;
      if (ir->type == IR::GOTO) {
        ++cnt;
        block.push_back({});
      }
    }
    ++cnt;

    // build flow graph
    vector<int> inDeg(cnt);
    vector<vector<int>> G(cnt);
    for (int u = 0, v; u < cnt - 1; ++u) {
      v = u + 1;
      if (block[u].back()->type == IR::GOTO)
        v = belong[block[u].back()->items[0]->ir->irId];
      ++inDeg[v];
      G[u].push_back(v);
    }
    for (auto &ir : func.second) {
      if (branch.count(ir->type)) {
        int v = belong[ir->items[0]->ir->irId];
        ++inDeg[v];
        G[belong[ir->irId]].push_back(v);
      }
    }

#ifdef DEBUG
    // debug
    printf("%s: %d\n", func.first->toString().data(), cnt);
    for (int i = 0; i < cnt; ++i) {
      printf("block %d:\n", i);
      for (auto ir : block[i])
        puts(ir->toString().data());
      puts("");
    }
    for (int i = 0; i < cnt; ++i)
      for (size_t k = 0; k < G[i].size(); ++k)
        printf("%d %d %d\n", i, G[i][k], (int)k);
    puts("\n\n");
#endif

    isym.clear();
    fsym.clear();

    // get used symbol in each loop
    auto col = Tarjan(G);
    vector<int> inLoop(cnt), vis(cnt);
    int scc = *max_element(col.begin(), col.end()) + 1;
    vector<set<Symbol *>> usedSymbol(scc);
    for (int u = 0; u < cnt; ++u) {
      int c = col[u];
      for (auto &ir : block[u])
        if (ir->type == IR::MOV)
          if (ir->items[0]->type == IRItem::SYMBOL)
            usedSymbol[c].insert(ir->items[0]->symbol);
      for (auto v : G[u])
        if (col[u] == col[v])
          inLoop[u] = 1;
    }

    // dfs flow graph
    function<void(int, int, int)> dfs = [&](int u, int p, int ban) {
      isym.clear();
      fsym.clear();
      if (vis[u])
        return;
      vis[u] = 1;
      if (!inLoop[p] && inLoop[u]) {
        for (auto s : usedSymbol[col[u]])
          if (s->dataType == Symbol::DataType::INT)
            isym.erase(s);
          else if (s->dataType == Symbol::DataType::FLOAT)
            fsym.erase(s);
      }
      block[u] = blockCP(block[u], ban ? usedSymbol[col[u]] : set<Symbol *>{});
      for (size_t i = 0; i < block[u].size(); ++i) {
        if (branch.count((block[u][i])->type)) {
          auto &ir = block[u][i];
          auto t1 = ir->items[1]->type;
          auto t2 = ir->items[2]->type;
          int res = -1;
          if (t1 == IRItem::INT && t2 == IRItem::INT)
            res = judge(ir->items[1]->iVal, ir->items[2]->iVal, ir->type);
          else if (t1 == IRItem::FLOAT && t2 == IRItem::FLOAT)
            res = judge(ir->items[1]->fVal, ir->items[2]->fVal, ir->type);
          if (res == 0) {
            block[u].erase(block[u].begin() + i);
            G[u] = {G[u][0]};
          } else if (res == 1) {
            ir->type = IR::GOTO;
            ir->items.resize(1);
            G[u] = {G[u][1]};
          }
        }
      }
      if (G[u].size() == 1)
        dfs(G[u][0], u, ban);
      else if (G[u].size() == 2) {
        for (auto v : G[u])
          for (auto s : usedSymbol[col[v]])
            if (s->dataType == Symbol::DataType::INT)
              isym.erase(s);
            else if (s->dataType == Symbol::DataType::FLOAT)
              fsym.erase(s);
        auto isymCopy = isym;
        auto fsymCopy = fsym;
        dfs(G[u][0], u, 1);
        isym = isymCopy;
        fsym = fsymCopy;
        dfs(G[u][1], u, 1);
      }
    };
    dfs(0, 0, 0);

#ifdef DEBUG
    printf("%s: %d\n", func.first->toString().data(), cnt);
    for (int i = 0; i < cnt; ++i) {
      printf("block %d:\n", i);
      for (auto ir : block[i])
        puts(ir->toString().data());
      puts("");
    }
#endif

    vector<IR *> result;
    for (int i = 0; i < cnt; ++i)
      if (vis[i])
        result.insert(result.end(), block[i].begin(), block[i].end());
    func.second = result;
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
      if (ir->type == IR::MOV && ir->items[0]->type == IRItem::RETURN)
        usedTemps.insert(ir->items[1]->iVal);
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
          ir->type == IR::MEMSET_ZERO) {
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
          Symbol *symbol = ir->items[i]->symbol;
          delete ir->items[i];
          ir->items[i] = new IRItem(
              symbol->dataType == Symbol::INT ? IRItem::ITEMP : IRItem::FTEMP,
              symbol2tempId[symbol]);
        }
      }
      irs.push_back(ir);
    }
    func.second = irs;
  }
}