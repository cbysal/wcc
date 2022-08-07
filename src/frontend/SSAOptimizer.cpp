#include "SSAOptimizer.h"
#include <numeric>
#include <vector>

using namespace std;

SSAOptimizer::SSAOptimizer(
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

SSAOptimizer::~SSAOptimizer() {
  for (IR *ir : toRecycleIRs)
    delete ir;
}

void SSAOptimizer::optimize() { isProcessed = true; }

vector<Symbol *> SSAOptimizer::getConsts() {
  if (!isProcessed)
    optimize();
  return consts;
}

vector<pair<Symbol *, vector<IR *>>> SSAOptimizer::getFuncs() {
  if (!isProcessed)
    optimize();
  return funcs;
}

vector<Symbol *> SSAOptimizer::getGlobalVars() {
  if (!isProcessed)
    optimize();
  return globalVars;
}

unordered_map<Symbol *, vector<Symbol *>>
SSAOptimizer::getLocalVars() {
  if (!isProcessed)
    optimize();
  return localVars;
}

unsigned SSAOptimizer::getTempId() { return tempId; }

vector<vector<int>> SSAOptimizer::dominatorTree::work(vector<vector<int>> &_G) {
  G = _G;
  n = G.size();
  sG.assign(n, {});
  iG.assign(n, {});
  for (int u = 0; u < n; ++u)
    for (auto v : G[u])
      iG[v].push_back(u);
  ord.clear(), ord.reserve(n);
  p.resize(n), dfn = fa = idom = p;
  iota(p.begin(), p.end(), 0), sdom = mn = p;
  return LengauerTarjan(0);
}

void SSAOptimizer::dominatorTree::dfs(int u) {
  dfn[u] = ++dft, ord.push_back(u);
  for (int v : G[u])
    if (!dfn[v])
      fa[v] = u, dfs(v);
}

int SSAOptimizer::dominatorTree::GF(int u) {
  if (u == p[u])
    return u;
  int &f = p[u], pu = GF(f);
  if (dfn[sdom[mn[f]]] < dfn[sdom[mn[u]]])
    mn[u] = mn[f];
  return f = pu;
}

vector<vector<int>> SSAOptimizer::dominatorTree::LengauerTarjan(int rt) {
  dfs(rt);
  for (int i = ord.size() - 1, u; i; --i) {
    u = ord[i];
    for (int v : iG[u]) {
      if (!dfn[v])
        continue;
      GF(v);
      if (dfn[sdom[mn[v]]] < dfn[sdom[u]])
        sdom[u] = sdom[mn[v]];
    }
    sG[sdom[u]].push_back(u);
    u = p[u] = fa[u];
    for (int v : sG[u])
      GF(v), idom[v] = u == sdom[mn[v]] ? u : mn[v];
    sG[u].clear();
  }
  for (int u : ord)
    if (idom[u] != sdom[u])
      idom[u] = idom[idom[u]];
  vector<vector<int>> res(n);
  for (int u = 1; u < n; ++u)
    res[idom[u]].push_back(u);
  return res;
}