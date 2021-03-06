#ifndef __SYNTAX_PARSER_H__
#define __SYNTAX_PARSER_H__

#include <stack>
#include <utility>
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
  vector<unordered_map<string, Symbol *>> symbolStack;

  void allocInitVal(vector<int>, unordered_map<int, AST *> &, int, AST *);
  void deleteInitVal(AST *);
  void initSymbols();
  Symbol *lastSymbol(string &);
  AST *parseAddExp();
  AST *parseAssignStmt();
  AST *parseBlock(Symbol *);
  vector<AST *> parseConstDef();
  template <typename T>
  void parseConstInitVal(vector<int>, unordered_map<int, T> &, int, AST *);
  AST *parseEqExp();
  AST *parseExpStmt();
  AST *parseFuncCall();
  AST *parseFuncDef();
  Symbol *parseFuncParam();
  vector<AST *> parseGlobalVarDef();
  AST *parseIfStmt(Symbol *);
  AST *parseInitVal();
  AST *parseLAndExp();
  AST *parseLOrExp();
  AST *parseLVal();
  vector<AST *> parseLocalVarDef();
  AST *parseMulExp();
  AST *parseRelExp();
  AST *parseReturnStmt(Symbol *);
  void parseRoot();
  AST *parseStmt(Symbol *);
  AST *parseUnaryExp();
  AST *parseWhileStmt(Symbol *);

public:
  SyntaxParser(vector<Token *> &tokens);
  ~SyntaxParser();

  AST *getAST();
  vector<Symbol *> &getSymbolTable();
};

#endif