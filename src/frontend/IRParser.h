#ifndef __IR_PARSER__
#define __IR_PARSER__

#include <utility>
#include <vector>

#include "AST.h"
#include "IR.h"
#include "Symbol.h"

using namespace std;

class IRParser {
private:
  bool isProcessed;
  AST *root;
  vector<Symbol *> symbols;
  vector<Symbol *> consts;
  vector<Symbol *> globalVars;
  unordered_map<Symbol *, vector<Symbol *>> localVars;
  vector<pair<Symbol *, vector<IR *>>> funcs;
  int tempId;

  vector<IR *> parseAST(AST *, Symbol *);
  vector<IR *> parseAlgoExp(AST *, Symbol *);
  vector<IR *> parseAssignStmt(AST *, Symbol *);
  vector<IR *> parseBinaryExp(AST *, Symbol *);
  vector<IR *> parseBlock(AST *, Symbol *);
  vector<IR *> parseCmpExp(AST *, Symbol *);
  vector<IR *> parseExpStmt(AST *, Symbol *);
  vector<IR *> parseFuncCall(AST *, Symbol *);
  vector<IR *> parseFuncDef(AST *, Symbol *);
  vector<IR *> parseIfStmt(AST *, Symbol *);
  vector<IR *> parseLAndExp(AST *, Symbol *);
  vector<IR *> parseLOrExp(AST *, Symbol *);
  vector<IR *> parseLVal(AST *, Symbol *);
  vector<IR *> parseModExp(AST *, Symbol *);
  vector<IR *> parseRVal(AST *, Symbol *);
  vector<IR *> parseReturnStmt(AST *, Symbol *);
  void parseRoot(AST *);
  vector<IR *> parseUnaryExp(AST *, Symbol *);
  vector<IR *> parseWhileStmt(AST *, Symbol *);

public:
  vector<Symbol *> getConsts();
  vector<Symbol *> getGlobalVars();
  unordered_map<Symbol *, vector<Symbol *>> getLocalVars();
  vector<pair<Symbol *, vector<IR *>>> getFuncs();
  void printIRs();

  IRParser(AST *, vector<Symbol *> &);
  ~IRParser();
};

#endif