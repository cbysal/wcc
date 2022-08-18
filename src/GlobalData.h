#include <unordered_map>
#include <vector>

#include "backend/ASM.h"
#include "frontend/AST.h"
#include "frontend/IR.h"
#include "frontend/Symbol.h"
#include "frontend/Token.h"

extern unsigned startLineno;
extern unsigned stopLineno;
extern std::vector<Token *> tokens;
extern AST *root;
extern std::vector<Symbol *> symbols;
extern std::vector<Symbol *> consts;
extern std::vector<Symbol *> globalVars;
extern std::unordered_map<Symbol *, std::vector<Symbol *>> localVars;
extern unsigned tempId;
extern std::unordered_map<Symbol *, std::vector<IR *>> funcIRs;
extern std::unordered_map<Symbol *, std::vector<ASM *>> funcASMs;

extern void initGlobalData();