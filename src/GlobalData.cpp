#include "GlobalData.h"

using namespace std;

unsigned startLineno;
unsigned stopLineno;
vector<Token *> tokens;
AST *root;
vector<Symbol *> symbols;
unordered_set<Symbol *> syscalls;
vector<Symbol *> consts;
vector<Symbol *> globalVars;
unordered_map<Symbol *, vector<Symbol *>> localVars;
unsigned tempId;
unordered_map<Symbol *, vector<IR *>> funcIRs;
unordered_map<Symbol *, vector<ASM *>> funcASMs;

void initGlobalData() {
  startLineno = 0;
  stopLineno = 0;
  tokens.clear();
  root = nullptr;
  symbols.clear();
  syscalls.clear();
  consts.clear();
  globalVars.clear();
  localVars.clear();
  tempId = 0;
  funcIRs.clear();
  funcASMs.clear();
}