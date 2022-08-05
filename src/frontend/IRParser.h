#ifndef __IR_PARSER__
#define __IR_PARSER__

#include <unordered_map>
#include <utility>
#include <vector>

#include "AST.h"
#include "IR.h"
#include "Symbol.h"

class IRParser {
private:
  bool isProcessed;
  AST *root;
  std::vector<Symbol *> symbols;
  std::vector<Symbol *> consts;
  std::vector<Symbol *> globalVars;
  std::unordered_map<Symbol *, std::vector<Symbol *>> localVars;
  std::vector<std::pair<Symbol *, std::vector<IR *>>> funcs;
  unsigned tempId;
  IR *funcEnd;

  IRItem *lastResult(const std::vector<IR *> &);
  std::vector<IR *> parseAST(AST *, Symbol *, IR *, IR *);
  std::vector<IR *> parseAlgoExp(AST *, Symbol *);
  std::vector<IR *> parseAssignStmt(AST *, Symbol *);
  std::vector<IR *> parseBlock(AST *, Symbol *, IR *, IR *);
  std::vector<IR *> parseCmpExp(AST *, Symbol *);
  std::vector<IR *> parseCond(AST *, Symbol *, IR *, IR *);
  std::vector<IR *> parseExpStmt(AST *, Symbol *);
  std::vector<IR *> parseFuncCall(AST *, Symbol *);
  std::vector<IR *> parseFuncDef(AST *, Symbol *);
  std::vector<IR *> parseIfStmt(AST *, Symbol *, IR *, IR *);
  std::vector<IR *> parseLAndExp(AST *, Symbol *, IR *, IR *);
  std::vector<IR *> parseLCmpExp(AST *, Symbol *, IR *, IR *);
  std::vector<IR *> parseLOrExp(AST *, Symbol *, IR *, IR *);
  std::vector<IR *> parseLVal(AST *, Symbol *);
  std::vector<IR *> parseRVal(AST *, Symbol *);
  std::vector<IR *> parseReturnStmt(AST *, Symbol *);
  void parseRoot(AST *);
  std::vector<IR *> parseWhileStmt(AST *, Symbol *);

public:
  std::vector<Symbol *> getConsts();
  std::vector<Symbol *> getGlobalVars();
  std::unordered_map<Symbol *, std::vector<Symbol *>> getLocalVars();
  std::vector<std::pair<Symbol *, std::vector<IR *>>> getFuncs();
  unsigned getTempId();

  IRParser(AST *, std::vector<Symbol *> &);
  ~IRParser();
};

#endif