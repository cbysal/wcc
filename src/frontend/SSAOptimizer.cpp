#include "SSAOptimizer.h"

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

std::unordered_map<Symbol *, std::vector<Symbol *>>
SSAOptimizer::getLocalVars() {
  if (!isProcessed)
    optimize();
  return localVars;
}

unsigned SSAOptimizer::getTempId() { return tempId; }
