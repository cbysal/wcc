#ifndef __GRAMMAR_PARSER_H__
#define __GRAMMAR_PARSER_H__

#include <stack>
#include <vector>

#include "AST.h"
#include "Symbol.h"
#include "Token.h"

using namespace std;

class GrammarParser {
private:
  AST *rootAST;
  bool isProcessed;
  unsigned head;
  vector<Token *> tokens;
  vector<Symbol *> symbols;

  int depth;
  vector<Symbol *> symbolStack;

  Symbol *lastSymbol(string);
  AST *parseAddExp();
  AST *parseAssignStmt();
  AST *parseBlock();
  vector<AST *> parseConstDef();
  template <typename T> void parseConstInitVal(vector<int>, T *, AST *);
  AST *parseEqExp();
  AST *parseExpStmt();
  AST *parseFuncCall();
  AST *parseFuncDef();
  AST *parseFuncFParam();
  AST *parseIfStmt();
  AST *parseInitVal();
  AST *parseLAndExp();
  AST *parseLOrExp();
  AST *parseLVal();
  AST *parseMulExp();
  AST *parseRelExp();
  AST *parseReturnStmt();
  void parseRoot();
  AST *parseStmt();
  AST *parseUnaryExp();
  vector<AST *> parseVarDef();
  AST *parseWhileStmt();

public:
  GrammarParser(const vector<Token *> tokens);
  ~GrammarParser();

  AST *getAST();
  vector<Symbol *> &getSymbolTable();
};

#endif