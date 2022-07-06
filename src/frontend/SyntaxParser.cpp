#include <algorithm>
#include <cstring>
#include <iostream>

#include "AST.h"
#include "SyntaxParser.h"

using namespace std;

SyntaxParser::SyntaxParser(vector<Token *> &tokens) {
  this->rootAST = nullptr;
  this->isProcessed = false;
  this->head = 0;
  this->tokens = tokens;
}

SyntaxParser::~SyntaxParser() {
  for (Symbol *&symbol : symbols)
    delete symbol;
  if (rootAST)
    delete rootAST;
}

void SyntaxParser::deleteInitVal(AST *root) {
  for (unsigned i = 0; i < root->nodes.size(); i++)
    if (root->nodes[i]->astType == AST::INIT_VAL)
      deleteInitVal(root->nodes[i]);
  root->nodes.clear();
  delete root;
}

void SyntaxParser::initSymbols() {
  symbolStack.resize(1);
  Symbol *func;
  Symbol *param1, *param2;
  // getint
  func = new Symbol(Symbol::FUNC, Symbol::INT, "getint", vector<Symbol *>());
  symbols.push_back(func);
  symbolStack.back()["getint"] = func;
  // getch
  func = new Symbol(Symbol::FUNC, Symbol::INT, "getch", vector<Symbol *>());
  symbols.push_back(func);
  symbolStack.back()["getch"] = func;
  // getarray
  param1 = new Symbol(Symbol::PARAM, Symbol::INT, "a", vector<int>{-1});
  func = new Symbol(Symbol::FUNC, Symbol::INT, "getarray",
                    vector<Symbol *>{param1});
  symbols.push_back(func);
  symbols.push_back(param1);
  symbolStack.back()["getarray"] = func;
  // getfloat
  func =
      new Symbol(Symbol::FUNC, Symbol::FLOAT, "getfloat", vector<Symbol *>());
  symbols.push_back(func);
  symbolStack.back()["getfloat"] = func;
  // getfarray
  param1 = new Symbol(Symbol::PARAM, Symbol::FLOAT, "a", vector<int>{-1});
  func = new Symbol(Symbol::FUNC, Symbol::INT, "getfarray",
                    vector<Symbol *>{param1});
  symbols.push_back(func);
  symbols.push_back(param1);
  symbolStack.back()["getfarray"] = func;
  // putint
  param1 = new Symbol(Symbol::PARAM, Symbol::INT, "a", vector<int>());
  func = new Symbol(Symbol::FUNC, Symbol::VOID, "putint",
                    vector<Symbol *>{param1});
  symbols.push_back(func);
  symbols.push_back(param1);
  symbolStack.back()["putint"] = func;
  // putch
  param1 = new Symbol(Symbol::PARAM, Symbol::INT, "a", vector<int>());
  func =
      new Symbol(Symbol::FUNC, Symbol::VOID, "putch", vector<Symbol *>{param1});
  symbols.push_back(func);
  symbols.push_back(param1);
  symbolStack.back()["putch"] = func;
  // putarray
  param1 = new Symbol(Symbol::PARAM, Symbol::INT, "n", vector<int>());
  param2 = new Symbol(Symbol::PARAM, Symbol::INT, "a", vector<int>{-1});
  func = new Symbol(Symbol::FUNC, Symbol::VOID, "putarray",
                    vector<Symbol *>{param1, param2});
  symbols.push_back(func);
  symbols.push_back(param1);
  symbols.push_back(param2);
  symbolStack.back()["putarray"] = func;
  // putfloat
  param1 = new Symbol(Symbol::PARAM, Symbol::FLOAT, "a", vector<int>());
  func = new Symbol(Symbol::FUNC, Symbol::VOID, "putfloat",
                    vector<Symbol *>{param1});
  symbols.push_back(func);
  symbols.push_back(param1);
  symbolStack.back()["putfloat"] = func;
  // putfarray
  param1 = new Symbol(Symbol::PARAM, Symbol::INT, "n", vector<int>());
  param2 = new Symbol(Symbol::PARAM, Symbol::FLOAT, "a", vector<int>{-1});
  func = new Symbol(Symbol::FUNC, Symbol::VOID, "putfarray",
                    vector<Symbol *>{param1, param2});
  symbols.push_back(func);
  symbols.push_back(param1);
  symbols.push_back(param2);
  symbolStack.back()["putfarray"] = func;
  // _sysy_starttime
  func = new Symbol(Symbol::FUNC, Symbol::VOID, "_sysy_starttime",
                    vector<Symbol *>());
  symbols.push_back(func);
  symbolStack.back()["starttime"] = func;
  // _sysy_stoptime
  func = new Symbol(Symbol::FUNC, Symbol::VOID, "_sysy_stoptime",
                    vector<Symbol *>());
  symbols.push_back(func);
  symbolStack.back()["stoptime"] = func;
}

Symbol *SyntaxParser::lastSymbol(string &name) {
  for (int i = symbolStack.size() - 1; i >= 0; i--)
    if (symbolStack[i].find(name) != symbolStack[i].end())
      return symbolStack[i][name];
  cerr << "no such function: " << name << endl;
  exit(-1);
  return nullptr;
}

AST *SyntaxParser::parseAddExp() {
  vector<AST *> items;
  vector<AST::OPType> ops;
  items.push_back(parseMulExp());
  while (tokens[head]->type == Token::MINUS ||
         tokens[head]->type == Token::PLUS) {
    ops.push_back(tokens[head]->type == Token::MINUS ? AST::SUB : AST::ADD);
    head++;
    items.push_back(parseMulExp());
  }
  while (!ops.empty()) {
    if ((items[0]->astType != AST::FLOAT_LITERAL &&
         items[0]->astType != AST::INT_LITERAL) ||
        (items[1]->astType != AST::FLOAT_LITERAL &&
         items[1]->astType != AST::INT_LITERAL))
      break;
    bool isInt1 = false;
    bool isInt2 = false;
    int intVal1 = 0;
    float floatVal1 = 0;
    int intVal2 = 0;
    float floatVal2 = 0;
    (isInt1 = items[0]->astType == AST::INT_LITERAL)
        ? (intVal1 = items[0]->iVal)
        : (floatVal1 = items[0]->fVal);
    (isInt2 = items[1]->astType == AST::INT_LITERAL)
        ? (intVal2 = items[1]->iVal)
        : (floatVal2 = items[1]->fVal);
    delete items[0];
    delete items[1];
    items.erase(items.begin() + 1);
    items[0] =
        ops[0] == AST::ADD
            ? (isInt1 && isInt2 ? new AST(intVal1 + intVal2)
                                : new AST((isInt1 ? intVal1 : floatVal1) +
                                          (isInt2 ? intVal2 : floatVal2)))
            : (isInt1 && isInt2 ? new AST(intVal1 - intVal2)
                                : new AST((isInt1 ? intVal1 : floatVal1) -
                                          (isInt2 ? intVal2 : floatVal2)));
    ops.erase(ops.begin());
  }
  for (unsigned i = 0; i < ops.size(); i++)
    if (items[i + 1]->astType == AST::UNARY_EXP &&
        items[i + 1]->opType == AST::NEG) {
      AST *val = items[i + 1]->nodes[0];
      items[i + 1]->nodes[0] = nullptr;
      delete items[i + 1];
      items[i + 1] = val;
      ops[i] = ops[i] == AST::ADD ? AST::SUB : AST::ADD;
    }
  AST *root = items[0];
  for (unsigned i = 0; i < ops.size(); i++) {
    AST *left = root;
    AST *right = items[i + 1];
    if (left->dataType == AST::INT && right->dataType == AST::FLOAT)
      left = new AST(AST::UNARY_EXP, AST::FLOAT, AST::I2F, {left});
    if (left->dataType == AST::FLOAT && right->dataType == AST::INT)
      right = new AST(AST::UNARY_EXP, AST::FLOAT, AST::I2F, {right});
    root = new AST(AST::BINARY_EXP, left->dataType, ops[i], {left, right});
  }
  return root;
}

AST *SyntaxParser::parseAssignStmt() {
  AST *lVal = parseLVal();
  head++;
  AST *rVal = parseAddExp();
  head++;
  if (lVal->dataType == AST::INT && rVal->dataType == AST::FLOAT)
    rVal = new AST(AST::UNARY_EXP, AST::INT, AST::F2I, {rVal});
  if (lVal->dataType == AST::FLOAT && rVal->dataType == AST::INT)
    rVal = new AST(AST::UNARY_EXP, AST::FLOAT, AST::I2F, {rVal});
  return new AST(AST::ASSIGN_STMT, AST::VOID, {lVal, rVal});
}

AST *SyntaxParser::parseBlock(Symbol *func) {
  vector<AST *> items;
  head++;
  while (tokens[head]->type != Token::RC) {
    switch (tokens[head]->type) {
    case Token::BREAK:
    case Token::CONTINUE:
    case Token::ID:
    case Token::IF:
    case Token::LC:
    case Token::RETURN:
    case Token::SEMICOLON:
    case Token::WHILE:
      items.push_back(parseStmt(func));
      break;
    case Token::CONST: {
      vector<AST *> consts = parseConstDef();
      items.insert(items.end(), consts.begin(), consts.end());
      break;
    }
    case Token::FLOAT:
    case Token::INT: {
      vector<AST *> vars = parseLocalVarDef();
      items.insert(items.end(), vars.begin(), vars.end());
      break;
    }
    default:
      break;
    }
  }
  head++;
  return new AST(AST::BLOCK, AST::VOID, items);
}

vector<AST *> SyntaxParser::parseConstDef() {
  head++;
  Symbol::DataType type =
      tokens[head]->type == Token::INT ? Symbol::INT : Symbol::FLOAT;
  head++;
  vector<AST *> consts;
  while (tokens[head]->type != Token::SEMICOLON) {
    string name = tokens[head]->sVal;
    head++;
    bool isArray = false;
    vector<int> dimensions;
    while (tokens[head]->type == Token::LB) {
      isArray = true;
      head++;
      AST *val = parseAddExp();
      dimensions.push_back(val->iVal);
      delete val;
      head++;
    }
    head++;
    AST *val = parseInitVal();
    Symbol *symbol = nullptr;
    if (isArray) {
      if (type == Symbol::INT) {
        unordered_map<int, int> iMap;
        parseConstInitVal(dimensions, iMap, 0, val);
        symbol = new Symbol(Symbol::CONST, type, name, dimensions, iMap);
      } else {
        unordered_map<int, float> fMap;
        parseConstInitVal(dimensions, fMap, 0, val);
        symbol = new Symbol(Symbol::CONST, type, name, dimensions, fMap);
      }
    } else
      symbol = val->astType == AST::INT_LITERAL
                   ? new Symbol(Symbol::CONST, type, name, val->iVal)
                   : new Symbol(Symbol::CONST, type, name, val->fVal);
    symbols.push_back(symbol);
    symbolStack.back()[name] = symbol;
    delete val;
    consts.push_back(new AST(AST::CONST_DEF, AST::VOID, symbol, {}));
    if (tokens[head]->type == Token::SEMICOLON)
      break;
    head++;
  }
  head++;
  return consts;
}

template <typename T>
void SyntaxParser::parseConstInitVal(vector<int> array,
                                     unordered_map<int, T> &tMap, int base,
                                     AST *src) {
  if (array.empty()) {
    while (src && src->astType == AST::INIT_VAL)
      src = src->nodes.empty() ? nullptr : src->nodes[0];
    if (src) {
      if (src->astType == AST::INT_LITERAL) {
        if (src->iVal)
          tMap[base] = src->iVal;
      } else {
        if (src->fVal)
          tMap[base] = src->fVal;
      }
    }
    return;
  }
  vector<int> index(array.size(), 0);
  vector<AST *> stk(src->nodes.rbegin(), src->nodes.rend());
  while (!stk.empty()) {
    if (stk.back()->astType != AST::INIT_VAL) {
      int offset = 0;
      for (unsigned i = 0; i < array.size(); i++)
        offset = offset * array[i] + index[i];
      if (stk.back()->astType == AST::INT_LITERAL) {
        if (stk.back()->iVal)
          tMap[base + offset] = stk.back()->iVal;
      } else {
        if (stk.back()->fVal)
          tMap[base + offset] = stk.back()->fVal;
      }
      index.back()++;
    } else {
      unsigned d = array.size() - 1;
      while (d > 0 && !index[d])
        d--;
      vector<int> newArray;
      for (unsigned i = d + 1; i < array.size(); i++)
        newArray.push_back(array[i]);
      int offset = 0;
      for (unsigned i = 0; i < array.size(); i++)
        offset = offset * array[i] + (i <= d ? index[i] : 0);
      parseConstInitVal(newArray, tMap, base + offset, stk.back());
      index[d]++;
    }
    for (int i = array.size() - 1; i >= 0 && index[i] >= array[i]; i--) {
      index[i] = 0;
      if (i == 0)
        return;
      index[i - 1]++;
    }
    stk.pop_back();
  }
}

AST *SyntaxParser::parseEqExp() {
  vector<AST *> items;
  vector<AST::OPType> ops;
  items.push_back(parseRelExp());
  while (tokens[head]->type == Token::EQ || tokens[head]->type == Token::NE) {
    ops.push_back(tokens[head]->type == Token::EQ ? AST::EQ : AST::NE);
    head++;
    items.push_back(parseRelExp());
  }
  while (!ops.empty()) {
    if ((items[0]->astType != AST::FLOAT_LITERAL &&
         items[0]->astType != AST::INT_LITERAL) ||
        (items[1]->astType != AST::FLOAT_LITERAL &&
         items[1]->astType != AST::INT_LITERAL))
      break;
    bool isInt1 = false;
    bool isInt2 = false;
    int intVal1 = 0;
    float floatVal1 = 0;
    int intVal2 = 0;
    float floatVal2 = 0;
    (isInt1 = items[0]->astType == AST::INT_LITERAL)
        ? (intVal1 = items[0]->iVal)
        : (floatVal1 = items[0]->fVal);
    (isInt2 = items[1]->astType == AST::INT_LITERAL)
        ? (intVal2 = items[1]->iVal)
        : (floatVal2 = items[1]->fVal);
    delete items[0];
    delete items[1];
    items.erase(items.begin() + 1);
    items[0] =
        ops[0] == AST::EQ
            ? (isInt1 && isInt2 ? new AST(intVal1 == intVal2)
                                : new AST((isInt1 ? intVal1 : floatVal1) ==
                                          (isInt2 ? intVal2 : floatVal2)))
            : (isInt1 && isInt2 ? new AST(intVal1 != intVal2)
                                : new AST((isInt1 ? intVal1 : floatVal1) !=
                                          (isInt2 ? intVal2 : floatVal2)));
    ops.erase(ops.begin());
  }
  AST *root = items[0];
  for (unsigned i = 0; i < ops.size(); i++) {
    AST *left = root;
    AST *right = items[i + 1];
    if (left->dataType == AST::INT && right->dataType == AST::FLOAT)
      left = new AST(AST::UNARY_EXP, AST::FLOAT, AST::I2F, {left});
    if (left->dataType == AST::FLOAT && right->dataType == AST::INT)
      right = new AST(AST::UNARY_EXP, AST::FLOAT, AST::I2F, {right});
    root = new AST(AST::BINARY_EXP, AST::INT, ops[i], {left, right});
  }
  return root;
}

AST *SyntaxParser::parseExpStmt() {
  AST *exp = parseAddExp();
  head++;
  return new AST(AST::EXP_STMT, AST::VOID, {exp});
}

AST *SyntaxParser::parseFuncCall() {
  Symbol *symbol = lastSymbol(tokens[head]->sVal);
  AST::DataType type = AST::VOID;
  switch (symbol->dataType) {
  case Symbol::FLOAT:
    type = AST::FLOAT;
    break;
  case Symbol::INT:
    type = AST::INT;
    break;
  case Symbol::VOID:
    type = AST::VOID;
    break;
  default:
    break;
  }
  head += 2;
  vector<AST *> params;
  while (tokens[head]->type != Token::RP) {
    AST *param = parseAddExp();
    if (symbol->params[params.size()]->dataType == Symbol::INT &&
        param->dataType == AST::FLOAT)
      param = new AST(AST::UNARY_EXP, AST::INT, AST::F2I, {param});
    if (symbol->params[params.size()]->dataType == Symbol::FLOAT &&
        param->dataType == AST::INT)
      param = new AST(AST::UNARY_EXP, AST::FLOAT, AST::I2F, {param});
    params.push_back(param);
    if (tokens[head]->type == Token::RP)
      break;
    head++;
  }
  head++;
  return new AST(AST::FUNC_CALL, type, symbol, {params});
}

AST *SyntaxParser::parseFuncDef() {
  Symbol::DataType type = Symbol::INT;
  switch (tokens[head]->type) {
  case Token::FLOAT:
    type = Symbol::FLOAT;
    break;
  case Token::INT:
    type = Symbol::INT;
    break;
  case Token::VOID:
    type = Symbol::VOID;
    break;
  default:
    break;
  }
  head++;
  string name = tokens[head]->sVal;
  head += 2;
  vector<Symbol *> params;
  while (tokens[head]->type != Token::RP) {
    params.push_back(parseFuncParam());
    if (tokens[head]->type == Token::RP)
      break;
    head++;
  }
  Symbol *symbol = new Symbol(Symbol::FUNC, type, name, params);
  symbols.push_back(symbol);
  symbolStack.back()[name] = symbol;
  head++;
  symbolStack.push_back({});
  symbols.insert(symbols.end(), params.begin(), params.end());
  for (Symbol *param : params)
    symbolStack.back()[param->name] = param;
  AST *body = parseBlock(symbol);
  symbolStack.pop_back();
  return new AST(AST::FUNC_DEF, AST::VOID, symbol, {body});
}

Symbol *SyntaxParser::parseFuncParam() {
  Symbol::DataType type =
      tokens[head]->type == Token::INT ? Symbol::INT : Symbol::FLOAT;
  head++;
  string name = tokens[head]->sVal;
  head++;
  if (tokens[head]->type != Token::LB)
    return new Symbol(Symbol::PARAM, type, name);
  vector<int> dimensions;
  dimensions.push_back(-1);
  head += 2;
  while (tokens[head]->type == Token::LB) {
    head++;
    AST *val = parseAddExp();
    dimensions.push_back(val->iVal);
    delete val;
    head++;
  }
  return new Symbol(Symbol::PARAM, type, name, dimensions);
}

vector<AST *> SyntaxParser::parseGlobalVarDef() {
  Symbol::DataType type =
      tokens[head]->type == Token::INT ? Symbol::INT : Symbol::FLOAT;
  head++;
  vector<AST *> consts;
  while (tokens[head]->type != Token::SEMICOLON) {
    string name = tokens[head]->sVal;
    head++;
    vector<int> dimensions;
    while (tokens[head]->type == Token::LB) {
      head++;
      AST *val = parseAddExp();
      dimensions.push_back(val->iVal);
      delete val;
      head++;
    }
    Symbol *symbol = nullptr;
    if (tokens[head]->type == Token::ASSIGN) {
      head++;
      AST *val = parseInitVal();
      if (dimensions.empty())
        symbol = val->astType == AST::INT_LITERAL
                     ? new Symbol(Symbol::GLOBAL_VAR, type, name, val->iVal)
                     : new Symbol(Symbol::GLOBAL_VAR, type, name, val->fVal);
      else if (type == Symbol::INT) {
        unordered_map<int, int> iMap;
        parseConstInitVal(dimensions, iMap, 0, val);
        symbol = new Symbol(Symbol::GLOBAL_VAR, type, name, dimensions, iMap);
      } else {
        unordered_map<int, float> fMap;
        parseConstInitVal(dimensions, fMap, 0, val);
        symbol = new Symbol(Symbol::GLOBAL_VAR, type, name, dimensions, fMap);
      }
      delete val;
    } else {
      if (dimensions.empty())
        symbol = new Symbol(Symbol::GLOBAL_VAR, type, name, 0);
      else if (type == Symbol::INT)
        symbol = new Symbol(Symbol::GLOBAL_VAR, type, name, dimensions);
      else
        symbol = new Symbol(Symbol::GLOBAL_VAR, type, name, dimensions);
    }
    symbols.push_back(symbol);
    symbolStack.back()[name] = symbol;
    consts.push_back(new AST(AST::GLOBAL_VAR_DEF, AST::VOID, symbol, {}));
    if (tokens[head]->type == Token::SEMICOLON)
      break;
    head++;
  }
  head++;
  return consts;
}

AST *SyntaxParser::parseIfStmt(Symbol *func) {
  head += 2;
  AST *cond = parseLOrExp();
  head++;
  bool depthInc = tokens[head]->type != Token::LC;
  if (depthInc)
    symbolStack.push_back({});
  AST *stmt1 = parseStmt(func);
  if (depthInc)
    symbolStack.pop_back();
  AST *stmt2 = nullptr;
  if (tokens[head]->type == Token::ELSE) {
    depthInc = tokens[head]->type != Token::LC;
    head++;
    if (depthInc)
      symbolStack.push_back({});
    stmt2 = parseStmt(func);
    if (depthInc)
      symbolStack.pop_back();
  }
  return new AST(AST::IF_STMT, AST::VOID, {cond, stmt1, stmt2});
}

AST *SyntaxParser::parseInitVal() {
  if (tokens[head]->type != Token::LC)
    return parseAddExp();
  head++;
  vector<AST *> items;
  while (tokens[head]->type != Token::RC) {
    items.push_back(parseInitVal());
    if (tokens[head]->type == Token::RC)
      break;
    head++;
  }
  head++;
  return new AST(AST::INIT_VAL, AST::VOID, items);
}

AST *SyntaxParser::parseLAndExp() {
  vector<AST *> items;
  items.push_back(parseEqExp());
  while (tokens[head]->type == Token::L_AND) {
    head++;
    items.push_back(parseEqExp());
  }
  while (items.size() > 1) {
    if ((items[0]->astType != AST::FLOAT_LITERAL &&
         items[0]->astType != AST::INT_LITERAL) ||
        (items[1]->astType != AST::FLOAT_LITERAL &&
         items[1]->astType != AST::INT_LITERAL))
      break;
    bool isInt1 = false;
    bool isInt2 = false;
    int intVal1 = 0;
    float floatVal1 = 0;
    int intVal2 = 0;
    float floatVal2 = 0;
    (isInt1 = items[0]->astType == AST::INT_LITERAL)
        ? (intVal1 = items[0]->iVal)
        : (floatVal1 = items[0]->fVal);
    (isInt2 = items[1]->astType == AST::INT_LITERAL)
        ? (intVal2 = items[1]->iVal)
        : (floatVal2 = items[1]->fVal);
    delete items[0];
    delete items[1];
    items.erase(items.begin() + 1);
    items[0] = new AST((isInt1 ? intVal1 : floatVal1) &&
                       (isInt2 ? intVal2 : floatVal2));
  }
  AST *root = items[0];
  for (unsigned i = 1; i < items.size(); i++)
    root = new AST(AST::BINARY_EXP, AST::VOID, AST::L_AND, {root, items[i]});
  return root;
}

AST *SyntaxParser::parseLOrExp() {
  vector<AST *> items;
  items.push_back(parseLAndExp());
  while (tokens[head]->type == Token::L_OR) {
    head++;
    items.push_back(parseLAndExp());
  }
  while (items.size() > 1) {
    if ((items[0]->astType != AST::FLOAT_LITERAL &&
         items[0]->astType != AST::INT_LITERAL) ||
        (items[1]->astType != AST::FLOAT_LITERAL &&
         items[1]->astType != AST::INT_LITERAL))
      break;
    bool isInt1 = false;
    bool isInt2 = false;
    int intVal1 = 0;
    float floatVal1 = 0;
    int intVal2 = 0;
    float floatVal2 = 0;
    (isInt1 = items[0]->astType == AST::INT_LITERAL)
        ? (intVal1 = items[0]->iVal)
        : (floatVal1 = items[0]->fVal);
    (isInt2 = items[1]->astType == AST::INT_LITERAL)
        ? (intVal2 = items[1]->iVal)
        : (floatVal2 = items[1]->fVal);
    delete items[0];
    delete items[1];
    items.erase(items.begin() + 1);
    items[0] = new AST((isInt1 ? intVal1 : floatVal1) ||
                       (isInt2 ? intVal2 : floatVal2));
  }
  AST *root = items[0];
  for (unsigned i = 1; i < items.size(); i++)
    root = new AST(AST::BINARY_EXP, AST::VOID, AST::L_OR, {root, items[i]});
  return root;
}

AST *SyntaxParser::parseLVal() {
  vector<int> array;
  string name = tokens[head]->sVal;
  Symbol *symbol = lastSymbol(name);
  head++;
  vector<AST *> dimensions;
  while (tokens[head]->type == Token::LB) {
    head++;
    dimensions.push_back(parseAddExp());
    if (dimensions.back()->astType == AST::INT_LITERAL)
      array.push_back(dimensions.back()->iVal);
    head++;
  }
  if (symbol->symbolType == Symbol::CONST &&
      array.size() == symbol->dimensions.size()) {
    for (AST *dimension : dimensions)
      delete dimension;
    if (symbol->dimensions.empty())
      return symbol->dataType == Symbol::INT ? new AST(symbol->iVal)
                                             : new AST(symbol->fVal);
    int offset = 0;
    for (unsigned i = 0; i < array.size(); i++)
      offset = offset * symbol->dimensions[i] + array[i];
    if (symbol->dataType == Symbol::INT)
      return new AST(symbol->iMap.find(offset) == symbol->iMap.end()
                         ? 0
                         : symbol->iMap[offset]);
    else
      return new AST(symbol->fMap.find(offset) == symbol->fMap.end()
                         ? 0
                         : symbol->fMap[offset]);
  }
  AST::DataType type = AST::VOID;
  switch (symbol->dataType) {
  case Symbol::FLOAT:
    type = AST::FLOAT;
    break;
  case Symbol::INT:
    type = AST::INT;
    break;
  case Symbol::VOID:
    type = AST::VOID;
    break;
  default:
    break;
  }
  return new AST(AST::L_VAL, type, symbol, dimensions);
}

AST *SyntaxParser::parseMulExp() {
  vector<AST *> items;
  vector<AST::OPType> ops;
  items.push_back(parseUnaryExp());
  while (tokens[head]->type == Token::DIV || tokens[head]->type == Token::MOD ||
         tokens[head]->type == Token::MUL) {
    switch (tokens[head]->type) {
    case Token::DIV:
      ops.push_back(AST::DIV);
      break;
    case Token::MOD:
      ops.push_back(AST::MOD);
      break;
    case Token::MUL:
      ops.push_back(AST::MUL);
      break;
    default:
      break;
    }
    head++;
    items.push_back(parseUnaryExp());
  }
  while (!ops.empty()) {
    if ((items[0]->astType != AST::FLOAT_LITERAL &&
         items[0]->astType != AST::INT_LITERAL) ||
        (items[1]->astType != AST::FLOAT_LITERAL &&
         items[1]->astType != AST::INT_LITERAL))
      break;
    bool isInt1 = false;
    bool isInt2 = false;
    int intVal1 = 0;
    float floatVal1 = 0;
    int intVal2 = 0;
    float floatVal2 = 0;
    (isInt1 = items[0]->astType == AST::INT_LITERAL)
        ? (intVal1 = items[0]->iVal)
        : (floatVal1 = items[0]->fVal);
    (isInt2 = items[1]->astType == AST::INT_LITERAL)
        ? (intVal2 = items[1]->iVal)
        : (floatVal2 = items[1]->fVal);
    delete items[0];
    delete items[1];
    items.erase(items.begin() + 1);
    switch (ops[0]) {
    case AST::DIV:
      items[0] = isInt1 && isInt2 ? new AST(intVal1 / intVal2)
                                  : new AST((isInt1 ? intVal1 : floatVal1) /
                                            (isInt2 ? intVal2 : floatVal2));
      break;
    case AST::MOD:
      items[0] = new AST(intVal1 % intVal2);
      break;
    case AST::MUL:
      items[0] = isInt1 && isInt2 ? new AST(intVal1 * intVal2)
                                  : new AST((isInt1 ? intVal1 : floatVal1) *
                                            (isInt2 ? intVal2 : floatVal2));
      break;
    default:
      break;
    }
    ops.erase(ops.begin());
  }
  AST *root = items[0];
  for (unsigned i = 0; i < ops.size(); i++) {
    AST *left = root;
    AST *right = items[i + 1];
    if (left->dataType == AST::INT && right->dataType == AST::FLOAT)
      left = new AST(AST::UNARY_EXP, AST::FLOAT, AST::I2F, {left});
    if (left->dataType == AST::FLOAT && right->dataType == AST::INT)
      right = new AST(AST::UNARY_EXP, AST::FLOAT, AST::I2F, {right});
    root = new AST(AST::BINARY_EXP, left->dataType, ops[i], {left, right});
  }
  return root;
}

AST *SyntaxParser::parseRelExp() {
  vector<AST *> items;
  vector<AST::OPType> ops;
  items.push_back(parseAddExp());
  while (tokens[head]->type == Token::GE || tokens[head]->type == Token::GT ||
         tokens[head]->type == Token::LE || tokens[head]->type == Token::LT) {
    switch (tokens[head]->type) {
    case Token::GE:
      ops.push_back(AST::GE);
      break;
    case Token::GT:
      ops.push_back(AST::GT);
      break;
    case Token::LE:
      ops.push_back(AST::LE);
      break;
    case Token::LT:
      ops.push_back(AST::LT);
      break;
    default:
      break;
    }
    head++;
    items.push_back(parseAddExp());
  }
  while (!ops.empty()) {
    if ((items[0]->astType != AST::FLOAT_LITERAL &&
         items[0]->astType != AST::INT_LITERAL) ||
        (items[1]->astType != AST::FLOAT_LITERAL &&
         items[1]->astType != AST::INT_LITERAL))
      break;
    bool isInt1 = false;
    bool isInt2 = false;
    int intVal1 = 0;
    float floatVal1 = 0;
    int intVal2 = 0;
    float floatVal2 = 0;
    (isInt1 = items[0]->astType == AST::INT_LITERAL)
        ? (intVal1 = items[0]->iVal)
        : (floatVal1 = items[0]->fVal);
    (isInt2 = items[1]->astType == AST::INT_LITERAL)
        ? (intVal2 = items[1]->iVal)
        : (floatVal2 = items[1]->fVal);
    delete items[0];
    delete items[1];
    items.erase(items.begin() + 1);
    switch (ops[0]) {
    case AST::GE:
      items[0] = new AST((isInt1 ? intVal1 : floatVal1) >=
                         (isInt2 ? intVal2 : floatVal2));
      break;
    case AST::GT:
      items[0] = new AST((isInt1 ? intVal1 : floatVal1) >
                         (isInt2 ? intVal2 : floatVal2));
      break;
    case AST::LE:
      items[0] = new AST((isInt1 ? intVal1 : floatVal1) <=
                         (isInt2 ? intVal2 : floatVal2));
      break;
    case AST::LT:
      items[0] = new AST((isInt1 ? intVal1 : floatVal1) <
                         (isInt2 ? intVal2 : floatVal2));
      break;
    default:
      break;
    }
    ops.erase(ops.begin());
  }
  AST *root = items[0];
  for (unsigned i = 0; i < ops.size(); i++) {
    AST *left = root;
    AST *right = items[i + 1];
    if (left->dataType == AST::INT && right->dataType == AST::FLOAT)
      left = new AST(AST::UNARY_EXP, AST::FLOAT, AST::I2F, {left});
    if (left->dataType == AST::FLOAT && right->dataType == AST::INT)
      right = new AST(AST::UNARY_EXP, AST::FLOAT, AST::I2F, {right});
    root = new AST(AST::BINARY_EXP, AST::INT, ops[i], {left, right});
  }
  return root;
}

AST *SyntaxParser::parseReturnStmt(Symbol *func) {
  head++;
  if (tokens[head]->type == Token::SEMICOLON) {
    head++;
    return new AST(AST::RETURN_STMT, AST::VOID, {});
  }
  AST *val = parseAddExp();
  head++;
  if (func->dataType == Symbol::INT && val->dataType == AST::FLOAT)
    val = new AST(AST::UNARY_EXP, AST::INT, AST::F2I, {val});
  if (func->dataType == Symbol::FLOAT && val->dataType == AST::INT)
    val = new AST(AST::UNARY_EXP, AST::FLOAT, AST::I2F, {val});
  return new AST(AST::RETURN_STMT, AST::VOID, {val});
}

void SyntaxParser::parseRoot() {
  if (isProcessed)
    return;
  isProcessed = true;
  initSymbols();
  vector<AST *> items;
  while (head < tokens.size()) {
    switch (tokens[head]->type) {
    case Token::CONST: {
      vector<AST *> consts = parseConstDef();
      items.insert(items.end(), consts.begin(), consts.end());
      break;
    }
    case Token::FLOAT:
    case Token::INT:
      switch (tokens[head + 2]->type) {
      case Token::ASSIGN:
      case Token::COMMA:
      case Token::LB:
      case Token::SEMICOLON: {
        vector<AST *> vars = parseGlobalVarDef();
        items.insert(items.end(), vars.begin(), vars.end());
        break;
      }
      case Token::LP:
        items.push_back(parseFuncDef());
        break;
      default:
        break;
      }
      break;
    case Token::VOID:
      items.push_back(parseFuncDef());
      break;
    default:
      break;
    }
  }
  rootAST = new AST(AST::ROOT, AST::VOID, items);
}

AST *SyntaxParser::parseStmt(Symbol *func) {
  switch (tokens[head]->type) {
  case Token::BREAK:
    head += 2;
    return new AST(AST::BREAK_STMT, AST::VOID, {});
  case Token::CONTINUE:
    head += 2;
    return new AST(AST::CONTINUE_STMT, AST::VOID, {});
  case Token::ID:
    switch (tokens[head + 1]->type) {
    case Token::ASSIGN:
    case Token::LB:
      return parseAssignStmt();
    case Token::DIV:
    case Token::LP:
    case Token::MINUS:
    case Token::MOD:
    case Token::MUL:
    case Token::PLUS:
      return parseExpStmt();
    default:
      break;
    }
  case Token::IF:
    return parseIfStmt(func);
  case Token::LC: {
    symbolStack.push_back({});
    AST *root = parseBlock(func);
    symbolStack.pop_back();
    return root;
  }
  case Token::LP:
  case Token::PLUS:
  case Token::MINUS:
  case Token::L_NOT:
    return parseExpStmt();
  case Token::RETURN:
    return parseReturnStmt(func);
  case Token::SEMICOLON:
    head++;
    return new AST(AST::BLANK_STMT, AST::VOID, {});
  case Token::WHILE:
    return parseWhileStmt(func);
  default:
    break;
  }
  return nullptr;
}

AST *SyntaxParser::parseUnaryExp() {
  switch (tokens[head]->type) {
  case Token::FLOAT_LITERAL: {
    float fVal = tokens[head]->fVal;
    head++;
    return new AST(fVal);
  }
  case Token::ID:
    if (tokens[head + 1]->type == Token::LP)
      return parseFuncCall();
    return parseLVal();
  case Token::INT_LITERAL: {
    int iVal = tokens[head]->iVal;
    head++;
    return new AST(iVal);
  }
  case Token::LP: {
    head++;
    AST *root = parseAddExp();
    head++;
    return root;
  }
  case Token::L_NOT: {
    head++;
    AST *val = parseUnaryExp();
    switch (val->astType) {
    case AST::FLOAT_LITERAL: {
      float fVal = val->fVal;
      delete val;
      return new AST(!fVal);
    }
    case AST::INT_LITERAL: {
      int iVal = val->iVal;
      delete val;
      return new AST(!iVal);
    }
    case AST::UNARY_EXP:
      switch (val->opType) {
      case AST::L_NOT: {
        AST *ret = val->nodes[0];
        val->nodes.clear();
        delete val;
        return ret;
      }
      case AST::NEG:
        return val;
      default:
        break;
      }
      break;
    default:
      break;
    }
    return new AST(AST::UNARY_EXP, AST::INT, AST::L_NOT, {val});
  }
  case Token::MINUS: {
    head++;
    AST *val = parseUnaryExp();
    switch (val->astType) {
    case AST::FLOAT_LITERAL:
      val->fVal = -val->fVal;
      return val;
    case AST::INT_LITERAL:
      val->iVal = -val->iVal;
      return val;
    case AST::UNARY_EXP:
      if (val->opType == AST::NEG) {
        AST *ret = val->nodes[0];
        val->nodes.clear();
        delete val;
        return ret;
      }
      break;
    default:
      break;
    }
    return new AST(AST::UNARY_EXP, val->dataType, AST::NEG, {val});
  }
  case Token::PLUS: {
    head++;
    return parseUnaryExp();
  }
  default:
    break;
  }
  return nullptr;
}

vector<AST *> SyntaxParser::parseLocalVarDef() {
  vector<AST *> items;
  Symbol::DataType type =
      tokens[head]->type == Token::INT ? Symbol::INT : Symbol::FLOAT;
  head++;
  while (tokens[head]->type != Token::SEMICOLON) {
    string name = tokens[head]->sVal;
    head++;
    vector<int> dimensions;
    while (tokens[head]->type == Token::LB) {
      head++;
      AST *val = parseAddExp();
      dimensions.push_back(val->iVal);
      delete val;
      head++;
    }
    Symbol *symbol = new Symbol(Symbol::LOCAL_VAR, type, name, dimensions);
    items.push_back(new AST(AST::LOCAL_VAR_DEF, AST::VOID, symbol, {}));
    if (tokens[head]->type == Token::ASSIGN) {
      head++;
      AST *val = parseInitVal();
      if (dimensions.empty())
        items.push_back(new AST(
            AST::ASSIGN_STMT, AST::VOID,
            {new AST(AST::L_VAL, type == Symbol::INT ? AST::INT : AST::FLOAT,
                     symbol, {}),
             val}));
      else {
        items.push_back(new AST(AST::MEMSET_ZERO, AST::VOID, symbol, {}));
        unordered_map<int, AST *> exps;
        allocInitVal(dimensions, exps, 0, val);
        vector<pair<int, AST *>> orderedExps(exps.begin(), exps.end());
        sort(orderedExps.begin(), orderedExps.end());
        for (pair<int, AST *> exp : orderedExps) {
          vector<AST *> dimensionASTs(dimensions.size());
          int t = exp.first;
          for (int j = dimensions.size() - 1; j >= 0; j--) {
            dimensionASTs[j] = new AST(t % dimensions[j]);
            t /= dimensions[j];
          }
          items.push_back(new AST(
              AST::ASSIGN_STMT, AST::VOID,
              {new AST(AST::L_VAL, type == Symbol::INT ? AST::INT : AST::FLOAT,
                       symbol, dimensionASTs),
               exp.second}));
        }
        deleteInitVal(val);
      }
    }
    symbols.push_back(symbol);
    symbolStack.back()[name] = symbol;
    if (tokens[head]->type == Token::SEMICOLON)
      break;
    head++;
  }
  head++;
  return items;
}

void SyntaxParser::allocInitVal(vector<int> array,
                                unordered_map<int, AST *> &exps, int base,
                                AST *src) {
  if (array.empty()) {
    while (src && src->astType == AST::INIT_VAL)
      src = src->nodes.empty() ? nullptr : src->nodes[0];
    if (src)
      exps[base] = src;
    return;
  }
  vector<int> index(array.size(), 0);
  vector<AST *> stk(src->nodes.rbegin(), src->nodes.rend());
  while (!stk.empty()) {
    if (stk.back()->astType != AST::INIT_VAL) {
      int offset = 0;
      for (unsigned i = 0; i < array.size(); i++)
        offset = offset * array[i] + index[i];
      if (stk.back())
        exps[base + offset] = stk.back();
      index.back()++;
    } else {
      unsigned d = array.size() - 1;
      while (d > 0 && !index[d])
        d--;
      vector<int> newArray;
      for (unsigned i = d + 1; i < array.size(); i++)
        newArray.push_back(array[i]);
      int offset = 0;
      for (unsigned i = 0; i < array.size(); i++)
        offset = offset * array[i] + (i <= d ? index[i] : 0);
      allocInitVal(newArray, exps, base + offset, stk.back());
      index[d]++;
    }
    for (int i = array.size() - 1; i >= 0 && index[i] >= array[i]; i--) {
      index[i] = 0;
      if (i == 0)
        return;
      index[i - 1]++;
    }
    stk.pop_back();
  }
}

AST *SyntaxParser::parseWhileStmt(Symbol *func) {
  head += 2;
  AST *cond = parseLOrExp();
  head++;
  bool depthInc = tokens[head]->type != Token::LC;
  if (depthInc)
    symbolStack.push_back({});
  AST *body = parseStmt(func);
  if (depthInc)
    symbolStack.pop_back();
  return new AST(AST::WHILE_STMT, AST::VOID, {cond, body});
}

AST *SyntaxParser::getAST() {
  if (!isProcessed)
    parseRoot();
  return rootAST;
};

vector<Symbol *> &SyntaxParser::getSymbolTable() {
  if (!isProcessed)
    parseRoot();
  return symbols;
}