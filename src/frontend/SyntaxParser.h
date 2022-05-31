#ifndef __SYNTAX_PARSER_H__
#define __SYNTAX_PARSER_H__

#include <stack>
#include <vector>

#include "AST.h"
#include "Symbol.h"
#include "Token.h"

using namespace std;

class SyntaxParser {
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
  vector<AST *> parseGlobalVarDef();
  AST *parseIfStmt();
  AST *parseInitVal();
  AST *parseLAndExp();
  AST *parseLOrExp();
  AST *parseLVal();
  vector<AST *> parseLocalVarDef();
  void allocInitVal(vector<int>, AST **, AST *);
  AST *parseMulExp();
  AST *parseRelExp();
  AST *parseReturnStmt();
  void parseRoot();
  AST *parseStmt();
  AST *parseUnaryExp();
  AST *parseWhileStmt();

public:
  SyntaxParser(const vector<Token *> tokens);
  ~SyntaxParser();

  AST *getAST();
  vector<Symbol *> &getSymbolTable();
};

#endif