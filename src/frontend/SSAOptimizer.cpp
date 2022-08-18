#include <functional>
#include <iostream>
#include <numeric>
#include <stack>
#include <unordered_set>

#include "../GlobalData.h"
#include "SSAOptimizer.h"

using namespace std;

void SSAOptimizer::process() {
  for (auto &func : funcIRs) {
    clear();
    funIR = func.second;
    funVar = localVars[func.first];
    preprocess();
    insertPhi();
    renameVariables(0);
    optimize();
    if (getenv("DEBUG"))
      debug(func.first);
    translateSSA();
  }
  clear();
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

  for (auto var : ssaVars)
    delete var;
  ssaVars.clear();

  csePhiTable.clear();
  cseExpTable.clear();
  cseCopyTable.clear();
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
  auto itemToString = [](SSAItem *it) {
    auto x = it->var;
    string y = ".v" + to_string(it->vid);
    switch (x->type) {
    case SSAvar::LOCAL:
      if (x->tmpID == -1)
        return x->symbol->name + y;
      else
        return "%" + to_string(x->tmpID) + y;
    case SSAvar::CONST:
      if (x->data == SSAvar::FLOAT)
        return to_string(x->fval);
      else
        return to_string(x->ival);
    case SSAvar::GLOBAL:
      return x->symbol->name + ".g";
    case SSAvar::RETURN:
      return string("ret");
    case SSAvar::LEBAL:
      return to_string(x->ival);
    case SSAvar::FUNC:
      return x->symbol->name;
    }
    return string();
  };
  string s;
  if (L != NULL)
    s += itemToString(L) + " = ";
  if (type != IR::MOV) {
    s += irTypeStr[type] + ' ';
    for (auto x : R)
      s += itemToString(x) + ", ";
    s.pop_back(), s.pop_back();
  } else {
    if (phi)
      s += "phi(";
    for (auto x : R)
      s += itemToString(x) + ", ";
    if (s.back() == ' ')
      s.pop_back(), s.pop_back();
    if (phi)
      s += ")";
  }
  return s;
}

void SSAOptimizer::debug(Symbol *f) {
  cout << "--------------------------------------------------------------------"
          "------------\n";
  cout << f->name << '\n';
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

SSAOptimizer::SSAvar::SSAvar() {
  symbol = NULL;
  data = INT;
  tmpID = -1;
  type = LOCAL;
}

SSAOptimizer::SSAvar::SSAvar(IRItem *x) {
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
    else if ((sType == Symbol::LOCAL_VAR && x->symbol->dimensions.empty()) ||
             sType == Symbol::PARAM) {
      type = LOCAL;
      tmpID = -1; // not temporary
    } else
      type = GLOBAL;
    data = x->symbol->dataType == Symbol::FLOAT ? FLOAT : INT;
    break;
  }
  }
}

SSAOptimizer::SSAItem::SSAItem(SSAvar *v, int i = 0) {
  var = v;
  vid = i;
}

SSAOptimizer::SSAIR::SSAIR(IR::IRType ty, SSAItem *l, vector<SSAItem *> r,
                           int isPhi = 0) {
  type = ty;
  L = l;
  R.swap(r);
  phi = isPhi;
}

SSAOptimizer::SSAIR *SSAOptimizer::newCopyIR(SSAItem *src) {
  SSAvar *tmp = new SSAvar;
  tmp->data = src->var->data;
  tmp->tmpID = (int)tmpVar.size();
  tmpVar[tmp->tmpID] = tmp;
  return new SSAIR(IR::MOV, new SSAItem(tmp), {src});
}

SSAOptimizer::SSAvar *SSAOptimizer::getSSAvar(IRItem *x) {
  switch (x->type) {
  case IRItem::RETURN:
    if (RET == NULL) {
      RET = new SSAvar(x);
      ssaVars.push_back(RET);
    }
    return RET;
  case IRItem::INT:
    if (!intCon.count(x->iVal)) {
      ssaVars.push_back(new SSAvar(x));
      intCon[x->iVal] = ssaVars.back();
    }
    return intCon[x->iVal];
  case IRItem::FLOAT:
    if (!floatCon.count(x->fVal)) {
      ssaVars.push_back(new SSAvar(x));
      floatCon[x->fVal] = ssaVars.back();
    }
    return floatCon[x->fVal];
  case IRItem::IR_T:
    if (!label.count(x->iVal)) {
      ssaVars.push_back(new SSAvar(x));
      label[x->iVal] = ssaVars.back();
    }
    return label[x->iVal];
  case IRItem::ITEMP:
  case IRItem::FTEMP:
    if (!tmpVar.count(x->iVal)) {
      ssaVars.push_back(new SSAvar(x));
      tmpVar[x->iVal] = ssaVars.back();
    }
    return tmpVar[x->iVal];
  default:
    if (!symVar.count(x->symbol)) {
      ssaVars.push_back(new SSAvar(x));
      symVar[x->symbol] = ssaVars.back();
    }
    return symVar[x->symbol];
  }
}

void SSAOptimizer::preprocess() {
  int cnt = 0;
  unordered_map<IR *, int> belong;

  // Part blocks: begin with label, end with goto or branch
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
    if (block[u].back()->type == IR::GOTO)
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
        var->type == SSAvar::CONST)
      ++vid;
    varVersion[var] = vid + 1;
    varStack[var].push(new SSAItem(var, vid)); // -1 represent undefined
  }
}

void SSAOptimizer::insertPhi() {
  // This indicates that phi-function for V has been inserted into X
  vector<set<SSAvar *>> inserted(block.size());
  // This represents that X has been added to W for variable V
  vector<set<SSAvar *>> work(block.size());
  for (auto &var : ssaVars) {
    if (var->type != SSAvar::LOCAL)
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
        ssaIRs[y].push_back(new SSAIR(IR::MOV, new SSAItem(var), {}, 1));
        // }
        if (!work[y].count(var)) {
          work[y].insert(var);
          W.insert(y);
        }
      }
    }
  }
}

void SSAOptimizer::renameVariables(int u) {
  unordered_map<SSAvar *, int> pushCnt;
  // Right-hand side of statement are all phi-function
  for (auto &ir : ssaIRs[u]) {
    if (!ir->phi)
      break;
    auto x = ir->L->var;
    ir->L = new SSAItem(x, varVersion[x]);
    varVersion[x] += 1;
    varStack[x].push(ir->L);
    pushCnt[x] += 1;
  }
  // Phi-function is not included on the right-hand side of statement
  for (auto &bir : block[u]) {
    if (bir->type == IR::LABEL)
      continue;
    int ins = 1;
    SSAItem *L = NULL;
    vector<SSAItem *> R;
    if (noDef.count(bir->type)) {
      for (auto x : bir->items)
        R.push_back(varStack[getSSAvar(x)].top());
    } else {
      for (size_t i = 1; i < bir->items.size(); ++i)
        R.push_back(varStack[getSSAvar(bir->items[i])].top());
      if (bir->type != IR::MOV && R.size() == 1)
        R.push_back(NULL);
      auto left = getSSAvar(bir->items[0]);
      if (left->type != SSAvar::LOCAL) {
        L = varStack[left].top();
      } else {
        // Copy Folding
        if (bir->type == IR::MOV && R.size() == 1 &&
            (R[0]->var->type == SSAvar::LOCAL ||
             R[0]->var->type == SSAvar::CONST)) {
          ins = 0;
          varStack[left].push(varStack[R[0]->var].top());
          pushCnt[left] += 1;
        } else {
          L = new SSAItem(left, varVersion[left]);
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
  for (auto v : T[u]) {
    renameVariables(v);
  }
  for (auto [var, cnt] : pushCnt)
    while (cnt--)
      varStack[var].pop();
}

void SSAOptimizer::optimize() {
  removeUselessPhi();
  copyPropagation();
  commonSubexpressionElimination(0);
  constantPropagation();
  deadCodeElimination();
}

int SSAOptimizer::isCopyIR(SSAIR *x) {
  return x->type == IR::MOV && x->R.size() == 1 &&
         x->L->var->type == SSAvar::LOCAL &&
         (x->R[0]->var->type == SSAvar::LOCAL ||
          x->R[0]->var->type == SSAvar::CONST);
}

void SSAOptimizer::removeUselessPhi() {
  while (1) {
    unordered_map<SSAItem *, SSAItem *> copy;
    for (auto &B : ssaIRs) {
      vector<SSAIR *> new_B;
      for (auto &ir : B)
        if (ir->phi) {
          unordered_set<SSAItem *> st;
          for (auto &it : ir->R)
            if (it != ir->L)
              st.insert(it);
          if (st.size() == 1) {
            copy[ir->L] = *st.begin();
          } else if (st.size() > 1) {
            ir->R = vector<SSAItem *>(st.begin(), st.end());
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
        for (auto &var : ir->R)
          if (copy.count(var))
            var = copy[var];
  }
}

void SSAOptimizer::copyPropagation() {
  unordered_map<SSAItem *, SSAItem *> copy;
  for (auto &B : ssaIRs) {
    vector<SSAIR *> new_B;
    for (auto &ir : B) {
      // copy statement
      if (isCopyIR(ir)) {
        copy[ir->L] = ir->R[0];
      } else
        new_B.push_back(ir);
    }
    B.swap(new_B);
  }
  if (copy.empty())
    return;
  for (auto &B : ssaIRs)
    for (auto &ir : B)
      for (auto &var : ir->R)
        if (copy.count(var))
          var = copy[var];
}

void SSAOptimizer::constantPropagation() {}

void SSAOptimizer::commonSubexpressionElimination(int u) {
  vector<SSAIR *> new_B;
  vector<SSAItem *> usedItem;
  vector<tuple<IR::IRType, SSAItem *, SSAItem *>> usedExp;
  vector<vector<SSAItem *>> usedPhi;
  for (auto &ir : ssaIRs[u]) {
    if (ir->phi) {
      if (csePhiTable.count(ir->R)) {
        cseCopyTable[ir->L] = csePhiTable[ir->R];
        usedItem.push_back(ir->L);
      } else {
        unordered_set<SSAItem *> st;
        for (auto &it : ir->R)
          if (it != ir->L)
            st.insert(it);
        if (st.size() == 1) {
          cseCopyTable[ir->L] = cseCopyTable[*st.begin()];
          usedItem.push_back(ir->L);
        } else if (st.size() > 1) {
          ir->R = vector<SSAItem *>(st.begin(), st.end());
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
      for (auto &it : ir->R)
        if (cseCopyTable.count(it))
          it = cseCopyTable[it];
      tuple<IR::IRType, SSAItem *, SSAItem *> Exp = {IR::MOV, NULL, NULL};
      if (ir->type != IR::MOV)
        Exp = {ir->type, ir->R[0], ir->R[1]};
      if (cseExpTable.count(Exp)) {
        ir->R = {cseExpTable[Exp]};
        ir->type = IR::MOV;
      }
      if (isCopyIR(ir)) {
        cseCopyTable[ir->L] = ir->R[0];
        usedItem.push_back(ir->L);
      } else
        new_B.push_back(ir);
      if (ir->type != IR::MOV) {
        cseExpTable[Exp] = ir->L;
        usedExp.push_back(Exp);
      } else if (ir->phi) {
        csePhiTable[ir->R] = ir->L;
        usedPhi.push_back(ir->R);
      }
    } else {
      for (auto &it : ir->R)
        if (cseCopyTable.count(it))
          it = cseCopyTable[it];
      new_B.push_back(ir);
    }
  }
  ssaIRs[u].swap(new_B);
  new_B.clear();
  for (auto v : G[u]) {
    for (auto &ir : ssaIRs[v])
      if (ir->phi)
        for (auto &it : ir->R)
          if (cseCopyTable.count(it))
            it = cseCopyTable[it];
  }
  for (auto v : T[u])
    commonSubexpressionElimination(v);
  for (auto &x : usedItem)
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

// Method of Briggs et al.
void SSAOptimizer::translateSSA() { preprocessForPhi(); }

void SSAOptimizer::preprocessForPhi() {
  // Replace constant and collect all phi-functions
  for (auto &B : ssaIRs) {
    vector<SSAIR *> new_B;
    for (auto &ir : B) {
      if (ir->phi) {
        for (auto &src : ir->R)
          if (src->var->type == SSAvar::CONST) {
            new_B.push_back(newCopyIR(src));
            src = new_B.back()->L;
          }
      }
      new_B.push_back(ir);
    }
    B.swap(new_B);
  }

  // Find the block to which the definition statement of each variable belongs
  for (size_t i = 0; i < ssaIRs.size(); ++i)
    for (auto &ir : ssaIRs[i])
      resrcBelong[ir->L] = i;
}