#include <functional>
#include <iostream>
#include <numeric>
#include <queue>
#include <stack>
#include <unordered_set>

#include "../GlobalData.h"
#include "SSAOptimizer.h"

using namespace std;

void SSAOptimizer::process() {
  tempId = 0;
  for (auto &func : funcIRs) {
    funSym = func.first;
    funIR = func.second;
    ssaConstruction();
    optimize();
    ssaDestruction();
    func.second.swap(funIR);
    if (getenv("DEBUG"))
      printSSA();
    clear();
  }
}

void SSAOptimizer::optimize() {
  removeUselessPhi();
  copyPropagation();
  if (getenv("DEBUG"))
    printSSA();
  commonSubexpressionElimination(0);
  constantPropagation();
  deadCodeElimination();
}

void SSAOptimizer::clear() {
  G.clear();
  block.clear();
  ssaIRs.clear();
  usedVar.clear();
  blockDef.clear();

  RET = NULL;
  floatCon.clear();
  intCon.clear();
  label.clear();
  tmpVar.clear();
  symVar.clear();

  varStack.clear();
  varVersion.clear();

  ssaVars.clear();

  csePhiTable.clear();
  cseExpTable.clear();
  cseCopyTable.clear();
}

SSAOptimizer::IRvar::IRvar() {
  symbol = NULL;
  data = INT;
  tmpID = -1;
  type = LOCAL;
}

SSAOptimizer::IRvar::IRvar(int x) {
  data = INT;
  type = CONST;
  ival = x;
  symbol = NULL;
}

SSAOptimizer::IRvar::IRvar(float x) {
  data = FLOAT;
  type = CONST;
  fval = x;
  symbol = NULL;
}

SSAOptimizer::IRvar::IRvar(IRItem *x) {
  symbol = NULL;
  switch (x->type) {
  case IRItem::INT:
    data = INT;
    ival = x->iVal;
    type = CONST;
    break;
  case IRItem::FLOAT:
    data = FLOAT;
    fval = x->fVal;
    type = CONST;
    break;
  case IRItem::FTEMP:
  case IRItem::ITEMP:
    tmpID = x->iVal;
    type = LOCAL;
    data = x->type == IRItem::FTEMP ? FLOAT : INT;
    break;
  case IRItem::IR_T:
    ival = x->iVal;
    type = LEBAL;
    break;
  case IRItem::RETURN:
    type = RETURN;
    break;
  default: {
    symbol = x->symbol;
    auto sType = symbol->symbolType;
    if (sType == Symbol::FUNC)
      type = FUNC;
    else if ((sType == Symbol::LOCAL_VAR || sType == Symbol::PARAM) &&
             x->symbol->dimensions.empty()) {
      type = LOCAL;
      tmpID = -1; // not temporary
    } else
      type = GLOBAL;
    data = x->symbol->dataType == Symbol::FLOAT ? FLOAT : INT;
    break;
  }
  }
}

SSAOptimizer::SSAvar::SSAvar(IRvar *v, int i = 0) {
  var = v;
  vid = i;
}

SSAOptimizer::SSAvar::SSAvar(SSAvar *a) {
  var = a->var;
  vid = a->vid;
}

SSAOptimizer::SSAItem::SSAItem(SSAvar *a = NULL, int b = 0) {
  x = a;
  def = b;
}

SSAOptimizer::SSAItem::SSAItem(IRvar *a, int id = 0, int b = 0) {
  x = new SSAvar(a, id);
  def = b;
}

vector<SSAOptimizer::SSAvar *> SSAOptimizer::SSAIR::to_SSAvarR() {
  vector<SSAvar *> res(R.size());
  for (size_t i = 0; i < R.size(); ++i)
    res[i] = R[i].x;
  return res;
}

SSAOptimizer::SSAIR::SSAIR(IR::IRType ty, SSAItem l, vector<SSAItem> r,
                           int isPhi = 0) {
  type = ty;
  L = l;
  R.swap(r);
  phi = isPhi;
}

SSAOptimizer::SSAIR *SSAOptimizer::newCopyIR(SSAItem src, int loc = -1) {
  int vid = varVersion[src->var];
  varVersion[src->var] += 1;
  // Add a copy instruction to the original place
  if (loc == -1)
    loc = src.def;
  return new SSAIR(IR::MOV, SSAItem(new SSAvar(src->var, vid), loc), {src});
}

SSAOptimizer::IRvar *SSAOptimizer::getSSAvar(IRItem *x) {
  switch (x->type) {
  case IRItem::RETURN:
    if (RET == NULL) {
      RET = new IRvar(x);
      ssaVars.push_back(RET);
    }
    return RET;
  case IRItem::INT:
    if (!intCon.count(x->iVal)) {
      ssaVars.push_back(new IRvar(x));
      intCon[x->iVal] = ssaVars.back();
    }
    return intCon[x->iVal];
  case IRItem::FLOAT:
    if (!floatCon.count(x->fVal)) {
      ssaVars.push_back(new IRvar(x));
      floatCon[x->fVal] = ssaVars.back();
    }
    return floatCon[x->fVal];
  case IRItem::IR_T:
    if (!label.count(x->iVal)) {
      ssaVars.push_back(new IRvar(x));
      label[x->iVal] = ssaVars.back();
    }
    return label[x->iVal];
  case IRItem::ITEMP:
  case IRItem::FTEMP:
    if (!tmpVar.count(x->iVal)) {
      ssaVars.push_back(new IRvar(x));
      tmpVar[x->iVal] = ssaVars.back();
    }
    return tmpVar[x->iVal];
  default:
    if (!symVar.count(x->symbol)) {
      ssaVars.push_back(new IRvar(x));
      symVar[x->symbol] = ssaVars.back();
    }
    return symVar[x->symbol];
  }
}

int SSAOptimizer::isCopyIR(SSAIR *x) {
  return x->type == IR::MOV && x->R.size() == 1 &&
         x->L->var->type == IRvar::LOCAL &&
         (x->R[0]->var->type == IRvar::LOCAL ||
          x->R[0]->var->type == IRvar::CONST);
}

string SSAOptimizer::SSAIR::toString() {
  static unordered_map<IR::IRType, string> irTypeStr = {
      {IR::ADD, "ADD"},     {IR::BEQ, "BEQ"},
      {IR::BGE, "BGE"},     {IR::BGT, "BGT"},
      {IR::BLE, "BLE"},     {IR::BLT, "BLT"},
      {IR::BNE, "BNE"},     {IR::CALL, "CALL"},
      {IR::DIV, "DIV"},     {IR::EQ, "EQ"},
      {IR::F2I, "F2I"},     {IR::GE, "GE"},
      {IR::GOTO, "GOTO"},   {IR::GT, "GT"},
      {IR::I2F, "I2F"},     {IR::L_NOT, "L_NOT"},
      {IR::LABEL, "LABEL"}, {IR::LE, "LE"},
      {IR::LT, "LT"},       {IR::MOD, "MOD"},
      {IR::MOV, "MOV"},     {IR::MUL, "MUL"},
      {IR::NE, "NE"},       {IR::NEG, "NEG"},
      {IR::SUB, "SUB"},     {IR::MEMSET_ZERO, "MEMSET_ZERO"}};
  auto itemToString = [](SSAvar *it) {
    if (!it)
      return string();
    auto x = it->var;
    string y = ".v" + to_string(it->vid);
    switch (x->type) {
    case IRvar::LOCAL:
      if (x->tmpID == -1)
        return x->symbol->name + y;
      else
        return "%" + to_string(x->tmpID) + y;
    case IRvar::CONST:
      if (x->data == IRvar::FLOAT)
        return to_string(x->fval);
      else
        return to_string(x->ival);
    case IRvar::GLOBAL:
      return x->symbol->name + ".g";
    case IRvar::RETURN:
      return string("ret");
    case IRvar::LEBAL:
      return to_string(x->ival);
    case IRvar::FUNC:
      return x->symbol->name;
    }
    return string();
  };
  string s;
  if (L.x != NULL)
    s += itemToString(L.x) + " = ";
  if (type != IR::MOV) {
    s += irTypeStr[type] + ' ';
    for (auto x : R)
      s += itemToString(x.x) + ", ";
    s.pop_back(), s.pop_back();
  } else if (phi) {
    s += "phi(";
    for (auto x : R)
      s += itemToString(x.x) + ":l" + to_string(x.def) + ", ";
    if (s.back() == ' ')
      s.pop_back(), s.pop_back();
    s += ")";
  } else {
    for (auto x : R)
      s += itemToString(x.x) + ", ";
    if (s.back() == ' ')
      s.pop_back(), s.pop_back();
  }
  return s;
}

void SSAOptimizer::printSSA() {
  cout << "--------------------------------------------------------------------"
          "------------\n";
  cout << funSym->name << '\n';
  cout << "Control Flow Graph:"
       << "\n";
  for (size_t u = 0; u < ssaIRs.size(); ++u)
    for (auto v : G[u])
      cout << u << " " << v << "\n";
  cout << "Dominator Tree:"
       << "\n";
  for (size_t u = 0; u < ssaIRs.size(); ++u)
    for (auto v : T[u])
      cout << u << " " << v << "\n";
  cout << "\n";
  for (size_t i = 0; i < ssaIRs.size(); ++i) {
    cout << "l" + to_string(i) + ":\n";
    for (auto &sir : ssaIRs[i])
      cout << "\t" << sir->toString() << "\n";
  }
  cout << "\n";
}

using graph = vector<vector<int>>;

void SSAOptimizer::dominatorTree::work(graph &_G, graph &_iG, graph &T,
                                       graph &DF) {
  G = _G;
  n = G.size();
  sG.assign(n, {});
  iG.assign(n, {});
  for (int u = 0; u < n; ++u)
    for (auto v : G[u])
      iG[v].push_back(u);
  _iG = iG;
  ord.clear(), ord.reserve(n);
  p.assign(n, 0), dfn = fa = p;
  idom.assign(n, -1);
  iota(p.begin(), p.end(), 0), sdom = mn = p;
  LengauerTarjan(0);
  T.assign(n, {});
  DF.assign(n, {});
  // Build dominator tree
  for (int u = 0; u < n; ++u)
    if (idom[u] != -1 && idom[u] != u)
      T[idom[u]].push_back(u);
  // Get dominator frontier
  function<void(int)> dfs = [&](int u) {
    for (int v : T[u]) {
      dfs(v);
      for (auto z : DF[v])
        if (idom[z] != u)
          DF[u].push_back(z);
    }
    for (int v : G[u])
      if (idom[v] != u)
        DF[u].push_back(v);
    sort(DF[u].begin(), DF[u].end());
    DF[u].erase(unique(DF[u].begin(), DF[u].end()), DF[u].end());
  };
  dfs(0);
}

void SSAOptimizer::dominatorTree::dfs(int u) {
  dfn[u] = ++dft, ord.push_back(u);
  for (int v : G[u])
    if (!dfn[v])
      fa[v] = u, dfs(v);
}

int SSAOptimizer::dominatorTree::getFther(int u) {
  if (u == p[u])
    return u;
  int &f = p[u], pu = getFther(f);
  if (dfn[sdom[mn[f]]] < dfn[sdom[mn[u]]])
    mn[u] = mn[f];
  return f = pu;
}

void SSAOptimizer::dominatorTree::LengauerTarjan(int rt) {
  dfs(rt);
  for (int i = ord.size() - 1, u; i; --i) {
    u = ord[i];
    for (int v : iG[u]) {
      if (!dfn[v])
        continue;
      getFther(v);
      if (dfn[sdom[mn[v]]] < dfn[sdom[u]])
        sdom[u] = sdom[mn[v]];
    }
    sG[sdom[u]].push_back(u);
    u = p[u] = fa[u];
    for (int v : sG[u])
      getFther(v), idom[v] = u == sdom[mn[v]] ? u : mn[v];
    sG[u].clear();
  }
  for (int u : ord)
    if (idom[u] != sdom[u])
      idom[u] = idom[idom[u]];
}

void SSAOptimizer::ssaConstruction() {
  int cnt = 1;
  unordered_map<IR *, int> belong;

  // Part blocks: begin with label, end with goto or branch
  block.push_back({}); // entry in empty
  block.push_back({});
  for (auto &ir : funIR) {
    if (!block.back().empty() && ir->type == IR::LABEL) {
      ++cnt;
      block.push_back({});
    }
    block.back().push_back(ir);
    belong[ir] = cnt;
    if (ir->type == IR::GOTO || branch.count(ir->type)) {
      ++cnt;
      block.push_back({});
    }
  }
  ++cnt;

  // Build flow graph
  G.resize(cnt);
  for (int u = 0, v; u < cnt - 1; ++u) {
    v = u + 1;
    if (!block[u].empty() && block[u].back()->type == IR::GOTO)
      v = belong[block[u].back()->items[0]->ir];
    G[u].push_back(v);
  }
  for (auto &ir : funIR)
    if (branch.count(ir->type)) {
      int u = belong[ir], v = belong[ir->items[0]->ir];
      G[u].push_back(v);
    }

  // Get dominator tree and dominance frontier
  domT.work(G, iG, T, DF);

  // Preprocess all variables used in function, variables used and defined by
  // each block. At the same time, convert the IR of each block to SSAIR
  usedVar.resize(block.size());
  ssaIRs.resize(block.size());
  for (size_t bid = 0; bid < block.size(); ++bid) {
    for (auto &bir : block[bid]) {
      if (bir->type == IR::LABEL)
        continue;
      for (size_t i = 0; i < bir->items.size(); ++i) {
        auto &it = bir->items[i];
        if (it->type == IRItem::IR_T)
          it->iVal = belong[it->ir];
        auto var = getSSAvar(it);
        if (it->type == IRItem::ITEMP || it->type == IRItem::FTEMP ||
            (it->type == IRItem::SYMBOL &&
             it->symbol->symbolType == Symbol::LOCAL_VAR &&
             it->symbol->dimensions.empty())) {
          usedVar[bid].insert(var);
          if (i == 0 && !noDef.count(bir->type)) {
            blockDef[var].push_back(bid);
          }
        }
      }
    }
  }

  // Initialize the stack into which variables will be put.
  for (auto &var : ssaVars) {
    int vid = -1;
    if ((var->symbol && var->symbol->symbolType == Symbol::PARAM) ||
        var->type == IRvar::CONST)
      ++vid;
    varVersion[var] = vid + 1;
    varStack[var].push(new SSAvar(var, vid)); // -1 represent undefined
  }

  // Insert phi-funtion
  // This indicates that phi-function for V has been inserted into X
  vector<set<IRvar *>> inserted(block.size());
  // This represents that X has been added to W for variable V
  vector<set<IRvar *>> work(block.size());
  for (auto &var : ssaVars) {
    if (var->type != IRvar::LOCAL)
      continue;
    set<int> W;
    for (auto bid : blockDef[var]) {
      W.insert(bid);
      work[bid].insert(var);
    }
    while (!W.empty()) {
      int x = *W.begin();
      W.erase(x);
      for (int y : DF[x]) {
        if (inserted[y].count(var))
          continue;
        // if (usedVar[y].count(var)) { // Pruned SSA
        inserted[y].insert(var);
        ssaIRs[y].push_back(new SSAIR(IR::MOV, new SSAvar(var), {}, 1));
        // }
        if (!work[y].count(var)) {
          work[y].insert(var);
          W.insert(y);
        }
      }
    }
  }

  renameVariables(0);
}

void SSAOptimizer::renameVariables(int u) {
  unordered_map<IRvar *, int> pushCnt;
  // Right-hand side of statement are all phi-function
  for (auto &ir : ssaIRs[u]) {
    if (!ir->phi)
      break;
    auto x = ir->L->var;
    ir->L = SSAItem(x, varVersion[x], u);
    varVersion[x] += 1;
    varStack[x].push(ir->L);
    pushCnt[x] += 1;
  }
  // Phi-function is not included on the right-hand side of statement
  for (auto &bir : block[u]) {
    if (bir->type == IR::LABEL)
      continue;
    int ins = 1;
    SSAItem L;
    vector<SSAItem> R;
    if (noDef.count(bir->type)) {
      for (auto x : bir->items)
        R.push_back(varStack[getSSAvar(x)].top());
    } else {
      for (size_t i = 1; i < bir->items.size(); ++i)
        R.push_back(varStack[getSSAvar(bir->items[i])].top());
      if (bir->type != IR::MOV && R.size() == 1)
        R.emplace_back();
      auto left = getSSAvar(bir->items[0]);
      if (left->type != IRvar::LOCAL) {
        L = {varStack[left].top().x, u};
      } else {
        // Copy Folding
        if (bir->type == IR::MOV && R.size() == 1 &&
            (R[0]->var->type == IRvar::LOCAL ||
             R[0]->var->type == IRvar::CONST)) {
          ins = 0;
          SSAItem right = {varStack[R[0]->var].top().x, u};
          varStack[left].push(right);
          pushCnt[left] += 1;
        } else {
          L = SSAItem(left, varVersion[left], u);
          varVersion[left] += 1;
          varStack[left].push(L);
          pushCnt[left] += 1;
        }
      }
    }
    if (ins)
      ssaIRs[u].push_back(new SSAIR(bir->type, L, R, 0));
  }
  for (auto v : G[u]) {
    for (auto &ir : ssaIRs[v]) {
      if (!ir->phi)
        break;
      if (varStack[ir->L->var].top()->vid != -1)
        ir->R.push_back(varStack[ir->L->var].top());
    }
  }
  for (auto v : T[u])
    renameVariables(v);
  for (auto [var, cnt] : pushCnt)
    while (cnt--)
      varStack[var].pop();
}

void SSAOptimizer::ssaDestruction() {
  vector<vector<SSAIR *>> backInsIR(ssaIRs.size());
  for (auto &B : ssaIRs) {
    vector<SSAIR *> new_B;
    for (auto &ir : B) {
      if (ir->phi) {
        auto new_ir = newCopyIR(ir->L);
        swap(ir->L, new_ir->L);
        auto L = ir->L;
        new_ir->R = {L};
        new_B.push_back(new_ir);
        for (auto &x : ir->R)
          backInsIR[x.def].push_back(new SSAIR(IR::MOV, L, {x}));
      } else
        new_B.push_back(ir);
    }
    B.swap(new_B);
  }

  for (size_t i = 0; i < ssaIRs.size(); ++i)
    if (!backInsIR[i].empty()) {
      auto &B = ssaIRs[i];
      auto back = B.empty() ? NULL : B.back();
      if (back && (branch.count(back->type) || back->type == IR::GOTO))
        B.pop_back();
      else
        back = NULL;
      B.insert(B.end(), backInsIR[i].begin(), backInsIR[i].end());
      if (back)
        B.push_back(back);
    }

  vector<vector<IR *>> block(ssaIRs.size());
  unordered_map<SSAvar *, int> itemMp;
  for (size_t i = 0; i < ssaIRs.size(); ++i)
    block[i].push_back(new IR(IR::LABEL));
  // Get all non array parameters: temp <- param
  for (auto &p : funSym->params) {
    if (p->dimensions.empty() && symVar.count(p)) {
      auto it = varStack[symVar[p]].top().x;
      itemMp[it] = tempId++;
      auto ir = new IR(IR::MOV);
      if (p->dataType == Symbol::INT) {
        ir->items.push_back(new IRItem(IRItem::ITEMP, itemMp[it]));
        ir->items.push_back(new IRItem(p));
      } else {
        ir->items.push_back(new IRItem(IRItem::FTEMP, itemMp[it]));
        ir->items.push_back(new IRItem(p));
      }
      block[0].push_back(ir);
    }
  }

  for (size_t i = 0; i < ssaIRs.size(); ++i) {
    auto &B = ssaIRs[i];
    for (auto &ir : B) {
      auto bir = new IR(ir->type);
      auto right = ir->to_SSAvarR();
      vector<SSAvar *> its;
      if (ir->L.x)
        its.push_back(ir->L.x);
      its.insert(its.end(), right.begin(), right.end());
      for (auto &t : its) {
        if (!t)
          continue;
        auto x = t->var;
        IRItem *y;
        switch (x->type) {
        case IRvar::CONST:
          if (x->data == IRvar::INT)
            y = new IRItem(x->ival);
          else
            y = new IRItem(x->fval);
          break;
        case IRvar::FUNC:
        case IRvar::GLOBAL:
          y = new IRItem(x->symbol);
          break;
        case IRvar::LEBAL:
          y = new IRItem(IRItem::IR_T);
          y->ir = block[x->ival][0];
          break;
        case IRvar::RETURN:
          y = new IRItem(IRItem::RETURN);
          break;
        default:
          // if (x->symbol && x->symbol->symbolType == Symbol::PARAM && t->vid
          // == 0)
          //   y = new IRItem(x->symbol);
          // else {
          if (!itemMp.count(t))
            itemMp[t] = tempId++;
          if (x->data == IRvar::INT)
            y = new IRItem(IRItem::ITEMP, itemMp[t]);
          else
            y = new IRItem(IRItem::FTEMP, itemMp[t]);
          // }
          break;
        }
        bir->items.push_back(y);
      }
      block[i].push_back(bir);
    }
  }

  funIR.clear();
  for (auto &b : block)
    funIR.insert(funIR.end(), b.begin(), b.end());
}

void SSAOptimizer::removeUselessPhi() {
  while (1) {
    unordered_map<SSAvar *, SSAItem> copy;
    for (size_t i = 0; i < ssaIRs.size(); ++i) {
      vector<SSAIR *> new_B, &B = ssaIRs[i];
      for (auto &ir : B)
        if (ir->phi) {
          set<SSAItem> st;
          for (auto &x : ir->R)
            if (x.x != ir->L.x)
              st.insert(x);
          if (st.size() == 1) {
            copy[ir->L.x] = {st.begin()->x, (int)i};
          } else if (st.size() > 1) {
            ir->R = vector<SSAItem>(st.begin(), st.end());
            new_B.push_back(ir);
          }
        } else
          new_B.push_back(ir);
      B.swap(new_B);
    }
    if (copy.empty())
      return;
    // copy propagation
    for (auto &B : ssaIRs)
      for (auto &ir : B)
        for (auto &x : ir->R)
          if (copy.count(x.x))
            x = copy[x.x];
  }
}

void SSAOptimizer::copyPropagation() {
  unordered_map<SSAvar *, SSAItem> copy;
  while (1) {
    copy.clear();
    for (size_t i = 0; i < ssaIRs.size(); ++i) {
      vector<SSAIR *> new_B, &B = ssaIRs[i];
      for (auto &ir : B) {
        // copy statement
        if (isCopyIR(ir)) {
          copy[ir->L.x] = {ir->R[0].x, (int)i};
        } else
          new_B.push_back(ir);
      }
      B.swap(new_B);
    }
    if (copy.empty())
      return;
    for (auto &B : ssaIRs)
      for (auto &ir : B)
        for (auto &x : ir->R)
          if (copy.count(x.x))
            x = copy[x.x];
  }
}

void SSAOptimizer::constantPropagation() {
  enum state {
    UNDEF, // undefined
    CONST, // constant
    INDET, // indeterminate
  };
  const unordered_set<IR::IRType> alu = {IR::ADD, IR::SUB, IR::MUL, IR::DIV,
                                         IR::MOD};
  queue<pair<int, int>> flowWork;
  queue<SSAvar *> ssaWork;
  set<pair<int, int>> reachEdge;
  vector<int> reachBlock;
  unordered_map<SSAvar *, state> varState;
  unordered_map<SSAvar *, int> intVal;
  unordered_map<SSAvar *, float> floatVal;
  // constant
  unordered_map<SSAIR *, int> bel;
  unordered_map<SSAvar *, int> def;
  unordered_map<SSAvar *, vector<SSAIR *>> usedExp;

  for (size_t i = 0; i < ssaIRs.size(); ++i)
    for (auto &ir : ssaIRs[i]) {
      bel[ir] = i;
      def[ir->L.x] = i;
      for (auto &x : ir->R)
        usedExp[x.x].push_back(ir);
    }
  
  auto phi_eval = [&](SSAIR *ir) {
    for (auto &x : ir->R)
      if (!reachEdge.count({x.def, ir->L.def}))
        return;
    int INDET = 0;
    for (auto &x : ir->R) {
      if (!x.x)
        continue;
      if (varState[x.x] == state::CONST) {
        if (x->var->data == IRvar::INT)
          x.x = new SSAvar(new IRvar(intVal[x.x]));
        else
          x.x = new SSAvar(new IRvar(floatVal[x.x]));
      }
      if (varState[x.x] == state::INDET)
        INDET = 1;
    }
    auto &st = varState[ir->L.x], pre = st;
    auto data = ir->L->var->data;
    auto s0 = varState[ir->R[0].x], s1 = varState[ir->R[1].x];
    // if (ir->R[0]->var->type == IRvar::CONST)
    //   s0 = state::CONST;
    // if (ir->R[1]->var->type == IRvar::CONST)
    //   s1 = state::CONST;
    auto &r0 = ir->R[0]->var, &r1 = ir->R[1]->var;
    if (INDET)
      st = state::INDET;
    else if (s0 == state::UNDEF && s1 == state::UNDEF)
      st = state::UNDEF;
    else if (s0 == state::UNDEF && s1 == state::CONST) {
      st = state::CONST;
      ir->phi = 0;
      if (data == IRvar::INT) {
        intVal[ir->L.x] = r0->ival;
      } else {
        floatVal[ir->L.x] = r0->fval;
      }
    } else if (s0 == state::CONST && s1 == state::UNDEF) {
      st = state::CONST;
      ir->phi = 0;
      if (data == IRvar::INT) {
        intVal[ir->L.x] = r1->ival;
      } else {
        floatVal[ir->L.x] = r1->fval;
      }
    } else if (data == IRvar::INT) {
      auto v0 = r0->ival;
      auto v1 = r1->ival;
      if (v0 == v1) {
        st = state::CONST;
        ir->phi = 0;
        intVal[ir->L.x] = v0;
      } else
        st = state::INDET;
    } else {
      auto v0 = r0->fval;
      auto v1 = r1->fval;
      if (v0 == v1) {
        st = state::CONST;
        ir->phi = 0;
        floatVal[ir->L.x] = v0;
      } else
        st = state::INDET;
    }
    if (st != pre)
      ssaWork.push(ir->L.x);
  };

  auto eval = [&](const auto &a, const auto &b, const IR::IRType &t) {
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

  auto exp_eval = [&](SSAIR *ir) {
    int CONST = 1, INDET = 0;
    for (auto &x : ir->R) {
      if (!x.x || x->var->type == IRvar::LEBAL)
        continue;
      if (varState[x.x] == state::CONST) {
        if (x->var->data == IRvar::INT)
          x.x = new SSAvar(new IRvar(intVal[x.x]));
        else
          x.x = new SSAvar(new IRvar(floatVal[x.x]));
      }
      if (x->var->type != IRvar::CONST)
        CONST = 0;
      if (x->var->type == IRvar::GLOBAL || x->var->type == IRvar::RETURN ||
          varState[x.x] == state::INDET)
        INDET = 1;
    }
    if (ir->L.x) {
      auto &st = varState[ir->L.x], pre = st;
      int data = ir->L->var->data;
      if (INDET)
        st = state::INDET;
      else if (CONST) {
        if (ir->type == IR::MOV) {
          if (ir->L->var->type == IRvar::GLOBAL || ir->L->var->type == IRvar::RETURN)
            st = state::INDET;
          else {
            st = state::CONST;
            auto &r = ir->R[0]->var;
            if (data == IRvar::INT) {
              intVal[ir->L.x] = r->ival;
            } else {
              floatVal[ir->L.x] = r->fval;
            }
          }
        } else {
          int &ival = intVal[ir->L.x];
          float &fval = floatVal[ir->L.x];
          auto &v0 = ir->R[0]->var;
          switch (ir->type) {
          case IR::L_NOT:
            if (v0->data == IRvar::INT)
              ival = !v0->ival;
            else
              ival = !v0->fval;
            break;
          case IR::F2I:
            ival = v0->fval;
            break;
          case IR::I2F:
            fval = v0->ival;
            break;
          case IR::NEG:
            if (v0->data == IRvar::INT)
              ival = -v0->ival;
            else
              fval = -v0->fval;
            break;
          default:
            auto &v1 = ir->R[1]->var;
            if (alu.count(ir->type)) {
              if (v0->data == IRvar::INT)
                ival = eval(v0->ival, v1->ival, ir->type);
              else
                fval = eval(v0->fval, v1->fval, ir->type);
            } else {
              if (v0->data == IRvar::INT)
                ival = judge(v0->ival, v1->ival, ir->type);
              else
                ival = judge(v0->fval, v1->fval, ir->type);
            }
            break;
          }
          st = state::CONST;
          ir->type = IR::MOV;
        }
      } else
        st = state::UNDEF;
      if (st != pre)
        ssaWork.push(ir->L.x);
    } else if (branch.count(ir->type)) {
      int u = bel[ir], res;
      if (!CONST) {
        for (auto v : G[u])
          flowWork.push({u, v});
        return;
      }
      auto &v0 = ir->R[1]->var, &v1 = ir->R[2]->var;
      if (v0->data == IRvar::INT)
        res = judge(v0->ival, v1->ival, ir->type);
      else
        res = judge(v0->fval, v1->fval, ir->type);
      ir->type = IR::GOTO;
      G[u] = {G[u][res]};
      ir->R = {ir->R[0]};
      ir->R[0]->var->ival = G[u][0];
      flowWork.push({u, G[u][0]});
    }
  };

  while (1) {
    varState.clear();
    reachEdge.clear();
    reachBlock.assign(ssaIRs.size(), 0);
    reachBlock[0] = 1;
    for (auto p : funSym->params)
      if (p->dimensions.empty() && symVar.count(p))
        varState[varStack[symVar[p]].top().x] = state::INDET;
    flowWork.push({0, 1});
    while (!flowWork.empty() || !ssaWork.empty()) {
      if (!flowWork.empty()) {
        auto [u, v] = flowWork.front();
        flowWork.pop();
        if (reachEdge.count({u, v}))
          continue;
        reachEdge.insert({u, v});
        for (auto ir : ssaIRs[v])
          if (ir->phi)
            phi_eval(ir);
        if (!reachBlock[v]) {
          reachBlock[v] = 1;
          for (auto ir : ssaIRs[v])
            if (!ir->phi)
              exp_eval(ir);
        }
        if (G[v].size() == 1)
          flowWork.push({v, G[v][0]});
      }
      if (!ssaWork.empty()) {
        auto v = ssaWork.front();
        ssaWork.pop();
        for (auto &ir : usedExp[v])
          if (reachBlock[bel[ir]]) {
            if (ir->phi)
              phi_eval(ir);
            else
              exp_eval(ir);
          }
      }
    }
    int ok = 0;
  
    // delete unreachable phi-function arguments
    for (auto &B : ssaIRs) {
      vector<SSAIR *> new_B;
      for (auto &ir : B)
        if (ir->phi) {
          vector<SSAItem> new_R;
          for (auto &x : ir->R)
            if (reachBlock[x.def])
              new_R.push_back(x);
            else
              ok = 1;
          ir->R.swap(new_R);
          if (ir->R.size() == 1)
            ir->phi = 0;
          if (ir->R.size() > 0)
            new_B.push_back(ir);
        } else
          new_B.push_back(ir);
      B.swap(new_B);
    }
    
    // delete unreachable block and constant mov IR
    for (size_t i = 0; i < ssaIRs.size(); ++i)
      if (!reachBlock[i])
        ssaIRs[i].clear();
      else {
        vector<SSAIR *> &B = ssaIRs[i], new_B;
        for (auto &ir : B)
          if (!ir->L.x || varState[ir->L.x] != state::CONST) {
            for (auto &x : ir->R)
              if (x->var && varState[x.x] == state::CONST) {
                if (x->var->data == IRvar::INT)
                  x.x = new SSAvar(new IRvar(intVal[x.x]));
                else
                  x.x = new SSAvar(new IRvar(floatVal[x.x]));
              }
            new_B.push_back(ir);
          }
        B.swap(new_B);
      }
    if (getenv("DEBUG"))
      printSSA();
    if (!ok)
      break;
  }
}

void SSAOptimizer::commonSubexpressionElimination(int u) {
  vector<SSAIR *> new_B;
  vector<SSAvar *> usedVar;
  vector<tuple<IR::IRType, SSAvar *, SSAvar *>> usedExp;
  vector<vector<SSAvar *>> usedPhi;
  for (auto &ir : ssaIRs[u]) {
    if (ir->phi) {
      auto right = ir->to_SSAvarR();
      if (csePhiTable.count(right)) {
        cseCopyTable[ir->L.x] = {csePhiTable[right].x, u};
        usedVar.push_back(ir->L.x);
      } else {
        set<SSAItem> st;
        for (auto &x : ir->R)
          if (x.x != ir->L.x)
            st.insert(x);
        if (st.size() == 1) {
          cseCopyTable[ir->L.x] = {cseCopyTable[st.begin()->x].x, u};
          usedVar.push_back(ir->L.x);
        } else if (st.size() > 1) {
          ir->R = vector<SSAItem>(st.begin(), st.end());
          new_B.push_back(ir);
        }
      }
    } else
      new_B.push_back(ir);
  }
  ssaIRs[u].swap(new_B);
  new_B.clear();
  for (auto &ir : ssaIRs[u]) {
    if (!noDef.count(ir->type)) {
      for (auto &x : ir->R)
        if (cseCopyTable.count(x.x))
          x = cseCopyTable[x.x];
      tuple<IR::IRType, SSAvar *, SSAvar *> Exp = {IR::MOV, NULL, NULL};
      if (ir->type != IR::MOV)
        Exp = {ir->type, ir->R[0].x, ir->R[1].x};
      if (cseExpTable.count(Exp)) {
        ir->R = {cseExpTable[Exp]};
        ir->type = IR::MOV;
      }
      if (isCopyIR(ir)) {
        cseCopyTable[ir->L.x] = {ir->R[0].x, u};
        usedVar.push_back(ir->L.x);
      } else
        new_B.push_back(ir);
      if (ir->type != IR::MOV) {
        cseExpTable[Exp] = ir->L;
        usedExp.push_back(Exp);
      } else if (ir->phi) {
        auto right = ir->to_SSAvarR();
        csePhiTable[right] = ir->L;
        usedPhi.push_back(right);
      }
    } else {
      for (auto &x : ir->R)
        if (cseCopyTable.count(x.x))
          x = cseCopyTable[x.x];
      new_B.push_back(ir);
    }
  }
  ssaIRs[u].swap(new_B);
  new_B.clear();
  for (auto v : G[u]) {
    for (auto &ir : ssaIRs[v])
      if (ir->phi)
        for (auto &x : ir->R)
          if (cseCopyTable.count(x.x))
            x = cseCopyTable[x.x];
  }
  for (auto v : T[u])
    commonSubexpressionElimination(v);
  for (auto &x : usedVar)
    cseCopyTable.erase(x);
  for (auto &x : usedExp)
    cseExpTable.erase(x);
  for (auto &x : usedPhi)
    csePhiTable.erase(x);
}

void SSAOptimizer::deadCodeElimination(){};

void SSAOptimizer::moveInvariantExpOut(){};

// Method of Sreedhar et al.
// void SSAOptimizer::translateSSA() {
//   preprocessForPhi();
//   // Phi Congruence Class
//   unordered_map<SSAItem* , set<SSAItem *>> pcc;
//   unordered_map<SSAItem* , vector<SSAIR *>> phiSrc;
//   // vector<pair<>>
//   for (auto &ir : phiIR) {
//     pcc[ir->L] = {ir->L};
//     for (auto &x : ir->R) {
//       pcc[x] = {x};
//       phiSrc[x].push_back(ir);
//     }
//   }

//   insIRs.resize(ssaIRs.size());

//   vector<set<SSAItem *>> LiveIn(ssaIRs.size()), LiveOut(ssaIRs.size());

//   for (auto &ir : phiIR) {
//     // Candidate Resource Set
//     set<SSAItem *> crs;
//     // Unresolved Neighbor Map
//     unordered_map<SSAItem *, set<SSAItem *>> unmp;
//     vector<SSAItem *> resrc = {ir->L};
//     resrc.insert(resrc.end(), ir->R.begin(), ir->R.end());
//     for (auto &xi : resrc)
//       for (auto &xj : resrc) {
//         if (xi == xj || pcc[xi] == pcc[xj])
//          continue;
//         int bi = resrcBelong[xi], bj = resrcBelong[xj];
//         auto process = [&] (set<SSAItem *> &Li, set<SSAItem *> &Lj) {
//           set<SSAItem *> sij, sji;
//           set_intersection(pcc[xi].begin(), pcc[xi].end(), Lj.begin(),
//           Lj.end(),
//                            inserter(sij, sij.begin()));
//           set_intersection(pcc[xj].begin(), pcc[xj].end(), Li.begin(),
//           Li.end(),
//                            inserter(sji, sji.begin()));
//           int state = sij.empty() << 1 | sji.empty();
//           switch (state) {
//           case 1:
//             crs.insert(xj);
//             break;
//           case 2:
//             crs.insert(xi);
//             break;
//           case 3:
//             crs.insert(xi);
//             crs.insert(xj);
//             break;
//           default:
//             unmp[xi].insert(xi);
//             unmp[xi].insert(xj);
//             break;
//           }
//         };
//         process(LiveIn[bi], LiveIn[bj]);
//         process(LiveIn[bi], LiveOut[bj]);
//         process(LiveOut[bi], LiveIn[bj]);
//         process(LiveOut[bi], LiveOut[bj]);
//       }

//     // Process the unresolved resources generated by defualt
//     for (auto &[var, mp] : unmp)
//       if (!crs.count(var))
//         for (auto &xi : mp)
//           if (!crs.count(xi)) {
//             crs.insert(var);
//             break;
//           }

//     // Insert copy statement
//     for (auto &xi : crs) {
//       for (auto &ir : phiSrc[xi]) {
//         int bel = resrcBelong[ir->L];
//         // Lk is the predecessor coresponding to xi
//         for (auto Lk : iG[bel]) {
//           auto cp = newCopyIR(xi);
//           auto &xnew = cp->L;
//           insIRs[Lk].push_back(cp);
//           for (auto &xj : ir->R)
//             if (xi == xj)
//               xj = xnew;
//           pcc[xnew] = {xnew};
//           LiveOut[Lk].insert(xnew);
//           int remove = 1;
//           for (auto Lj : G[Lk]) {
//             if (LiveIn[Lj].count(xi))
//               remove = 0;
//           }
//         }
//       }
//     }
//   }
// }