#ifndef __IR_PARSER__
#define __IR_PARSER__

#include <utility>
#include <vector>

#include "AST.h"
#include "IR.h"
#include "Symbol.h"

using namespace std;

class IRParser {
public:
  bool isProcessed;
  AST *root;
  vector<Symbol *> symbols;
  vector<Symbol *> consts;
  vector<Symbol *> globalVars;
  vector<pair<Symbol *, vector<IR *>>> funcs;
  int tempId;

  vector<IR *> parseAlgoExp(AST *);
  vector<IR *> parseAssignStmt(AST *);
  vector<IR *> parseAST(AST *);
  vector<IR *> parseBinaryExp(AST *);
  vector<IR *> parseBlock(AST *);
  vector<IR *> parseCmpExp(AST *);
  vector<IR *> parseExpStmt(AST *);
  vector<IR *> parseFuncCall(AST *);
  vector<IR *> parseFuncDef(AST *);
  vector<IR *> parseIfStmt(AST *);
  vector<IR *> parseLAndExp(AST *);
  vector<IR *> parseLOrExp(AST *);
  vector<IR *> parseLVal(AST *);
  vector<IR *> parseReturnStmt(AST *);
  void parseRoot(AST *);
  vector<IR *> parseUnaryExp(AST *);
  vector<IR *> parseWhileStmt(AST *);

  void printIRs();

  IRParser(AST *, vector<Symbol *> &);
  ~IRParser();
};

#endif