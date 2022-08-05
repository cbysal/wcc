#ifndef __SYNTAX_PARSER_H__
#define __SYNTAX_PARSER_H__

#include <string>
#include <unordered_map>
#include <vector>

#include "AST.h"
#include "Symbol.h"
#include "Token.h"

class SyntaxParser {
private:
  AST *rootAST;
  bool isProcessed;
  unsigned head;
  std::vector<Token *> tokens;
  std::vector<Symbol *> symbols;
  std::vector<std::unordered_map<std::string, Symbol *>> symbolStack;

  void allocInitVal(std::vector<int>, std::unordered_map<int, AST *> &, int,
                    AST *);
  void deleteInitVal(AST *);
  void initSymbols();
  Symbol *lastSymbol(std::string &);
  AST *parseAddExp();
  AST *parseAssignStmt();
  AST *parseBlock(Symbol *);
  std::vector<AST *> parseConstDef();
  template <typename T>
  void parseConstInitVal(std::vector<int>, std::unordered_map<int, T> &, int,
                         AST *);
  AST *parseEqExp();
  AST *parseExpStmt();
  AST *parseFuncCall();
  AST *parseFuncDef();
  Symbol *parseFuncParam();
  std::vector<AST *> parseGlobalVarDef();
  AST *parseIfStmt(Symbol *);
  AST *parseInitVal();
  AST *parseLAndExp();
  AST *parseLOrExp();
  AST *parseLVal();
  std::vector<AST *> parseLocalVarDef();
  AST *parseMulExp();
  AST *parseRelExp();
  AST *parseReturnStmt(Symbol *);
  void parseRoot();
  AST *parseStmt(Symbol *);
  AST *parseUnaryExp();
  AST *parseWhileStmt(Symbol *);

public:
  SyntaxParser(std::vector<Token *> &tokens);
  ~SyntaxParser();

  AST *getAST();
  std::vector<Symbol *> &getSymbolTable();
};

#endif