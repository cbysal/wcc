#include <algorithm>
#include <cstring>
#include <iostream>
#include <utility>

#include "../GlobalData.h"
#include "AST.h"
#include "SyntaxParser.h"

using namespace std;

SyntaxParser::SyntaxParser() { this->head = 0; }

void SyntaxParser::allocInitVal(vector<int> array,
                                unordered_map<int, AST *> &exps, int base,
                                AST *src) {
  if (array.empty()) {
    while (src && src->type == AST::INIT_VAL)
      src = src->nodes.empty() ? nullptr : src->nodes[0];
    if (src)
      exps[base] = src;
    return;
  }
  vector<int> index(array.size(), 0);
  vector<AST *> stk(src->nodes.rbegin(), src->nodes.rend());
  while (!stk.empty()) {
    if (stk.back()->type != AST::INIT_VAL) {
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
  // starttime
  func = new Symbol(Symbol::FUNC, Symbol::VOID, "_sysy_starttime",
                    vector<Symbol *>());
  symbols.push_back(func);
  symbolStack.back()["starttime"] = func;
  // stoptime
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
  AST *left = parseMulExp();
  while (tokens[head]->type == Token::MINUS ||
         tokens[head]->type == Token::PLUS) {
    AST::ASTType type = AST::ERR_TYPE;
    switch (tokens[head]->type) {
    case Token::MINUS:
      type = AST::SUB_EXP;
      break;
    case Token::PLUS:
      type = AST::ADD_EXP;
      break;
    default:
      break;
    }
    head++;
    AST *right = parseMulExp();
    if ((left->type == AST::FLOAT || left->type == AST::INT) &&
        (right->type == AST::FLOAT || right->type == AST::INT)) {
      switch (type) {
      case AST::ADD_EXP:
        if (left->type == AST::FLOAT || right->type == AST::FLOAT) {
          left->isFloat = true;
          left->fVal = (left->type == AST::FLOAT ? left->fVal : left->iVal) +
                       (right->type == AST::FLOAT ? right->fVal : right->iVal);
          left->type = AST::FLOAT;
        } else
          left->iVal = left->iVal + right->iVal;
        break;
      case AST::SUB_EXP:
        if (left->type == AST::FLOAT || right->type == AST::FLOAT) {
          left->isFloat = true;
          left->fVal = (left->type == AST::FLOAT ? left->fVal : left->iVal) -
                       (right->type == AST::FLOAT ? right->fVal : right->iVal);
          left->type = AST::FLOAT;
        } else
          left->iVal = left->iVal - right->iVal;
        break;
      default:
        break;
      }
      continue;
    }
    if (!left->isFloat && right->isFloat)
      left = left->transIF();
    if (left->isFloat && !right->isFloat)
      right = right->transIF();
    left = new AST(type, left->isFloat, {left, right});
  }
  return left;
}

AST *SyntaxParser::parseAssignStmt() {
  AST *lVal = parseLVal();
  head++;
  AST *rVal = parseAddExp();
  head++;
  if (lVal->isFloat ^ rVal->isFloat)
    rVal = rVal->transIF();
  return new AST(AST::ASSIGN_STMT, false, {lVal, rVal});
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
  return new AST(AST::BLOCK, false, items);
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
      head++;
    }
    head++;
    AST *val = parseInitVal();
    Symbol *symbol = nullptr;
    if (isArray) {
      if (type == Symbol::FLOAT) {
        unordered_map<int, float> fMap;
        parseConstInitVal(dimensions, fMap, 0, val);
        symbol = new Symbol(Symbol::CONST, type, name, dimensions, fMap);
      } else {
        unordered_map<int, int> iMap;
        parseConstInitVal(dimensions, iMap, 0, val);
        symbol = new Symbol(Symbol::CONST, type, name, dimensions, iMap);
      }
    } else
      symbol = val->type == AST::FLOAT
                   ? new Symbol(Symbol::CONST, type, name, val->fVal)
                   : new Symbol(Symbol::CONST, type, name, val->iVal);
    symbols.push_back(symbol);
    symbolStack.back()[name] = symbol;
    consts.push_back(new AST(AST::CONST_DEF, false, symbol, {}));
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
    while (src && src->type == AST::INIT_VAL)
      src = src->nodes.empty() ? nullptr : src->nodes[0];
    if (src) {
      if (src->type == AST::INT) {
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
    if (stk.back()->type != AST::INIT_VAL) {
      int offset = 0;
      for (unsigned i = 0; i < array.size(); i++)
        offset = offset * array[i] + index[i];
      if (stk.back()->type == AST::INT) {
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
  AST *left = parseRelExp();
  while (tokens[head]->type == Token::EQ || tokens[head]->type == Token::NE) {
    AST::ASTType type = AST::ERR_TYPE;
    switch (tokens[head]->type) {
    case Token::EQ:
      type = AST::EQ_EXP;
      break;
    case Token::NE:
      type = AST::NE_EXP;
      break;
    default:
      break;
    }
    head++;
    AST *right = parseRelExp();
    if ((left->type == AST::FLOAT || left->type == AST::INT) &&
        (right->type == AST::FLOAT || right->type == AST::INT)) {
      left->isFloat = false;
      switch (type) {
      case AST::EQ_EXP:
        left->isFloat = false;
        left->iVal =
            (left->type == AST::FLOAT || right->type == AST::FLOAT)
                ? ((left->type == AST::FLOAT ? left->fVal : left->iVal) ==
                   (right->type == AST::FLOAT ? right->fVal : right->iVal))
                : (left->iVal == right->iVal);
        left->type = AST::INT;
        break;
      case AST::NE_EXP:
        left->isFloat = false;
        left->iVal =
            (left->type == AST::FLOAT || right->type == AST::FLOAT)
                ? ((left->type == AST::FLOAT ? left->fVal : left->iVal) !=
                   (right->type == AST::FLOAT ? right->fVal : right->iVal))
                : (left->iVal != right->iVal);
        left->type = AST::INT;
        break;
      default:
        break;
      }
      continue;
    }
    if (!left->isFloat && right->isFloat)
      left = left->transIF();
    if (left->isFloat && !right->isFloat)
      right = right->transIF();
    left = new AST(type, false, {left, right});
  }
  return left;
}

AST *SyntaxParser::parseExpStmt() {
  AST *exp = parseAddExp();
  head++;
  return new AST(AST::EXP_STMT, false, {exp});
}

AST *SyntaxParser::parseFuncCall() {
  Symbol *symbol = lastSymbol(tokens[head]->sVal);
  head += 2;
  vector<AST *> params;
  while (tokens[head]->type != Token::RP) {
    AST *param = parseAddExp();
    if ((symbol->params[params.size()]->dataType == Symbol::FLOAT) ^
        param->isFloat)
      param = param->transIF();
    params.push_back(param);
    if (tokens[head]->type == Token::RP)
      break;
    head++;
  }
  head++;
  return new AST(AST::FUNC_CALL, symbol->dataType == Symbol::FLOAT, symbol,
                 {params});
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
  return new AST(AST::FUNC_DEF, false, symbol, {body});
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
      head++;
    }
    Symbol *symbol = nullptr;
    if (tokens[head]->type == Token::ASSIGN) {
      head++;
      AST *val = parseInitVal();
      if (dimensions.empty())
        symbol = val->type == AST::FLOAT
                     ? new Symbol(Symbol::GLOBAL_VAR, type, name, val->fVal)
                     : new Symbol(Symbol::GLOBAL_VAR, type, name, val->iVal);
      else if (type == Symbol::FLOAT) {
        unordered_map<int, float> fMap;
        parseConstInitVal(dimensions, fMap, 0, val);
        symbol = new Symbol(Symbol::GLOBAL_VAR, type, name, dimensions, fMap);
      } else {
        unordered_map<int, int> iMap;
        parseConstInitVal(dimensions, iMap, 0, val);
        symbol = new Symbol(Symbol::GLOBAL_VAR, type, name, dimensions, iMap);
      }
    } else {
      if (dimensions.empty())
        symbol = new Symbol(Symbol::GLOBAL_VAR, type, name, 0);
      else
        symbol = new Symbol(Symbol::GLOBAL_VAR, type, name, dimensions);
    }
    symbols.push_back(symbol);
    symbolStack.back()[name] = symbol;
    consts.push_back(new AST(AST::GLOBAL_VAR_DEF, false, symbol, {}));
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
  if (cond->type == AST::FLOAT || cond->type == AST::INT) {
    if ((cond->type == AST::FLOAT && cond->fVal) ||
        (cond->type == AST::INT && cond->iVal))
      return stmt1;
    if (stmt2)
      return stmt2;
    else
      return new AST(AST::BLANK_STMT, false, {});
  }
  return new AST(AST::IF_STMT, false, {cond, stmt1, stmt2});
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
  return new AST(AST::INIT_VAL, false, items);
}

AST *SyntaxParser::parseLAndExp() {
  AST *left = parseEqExp();
  while (tokens[head]->type == Token::L_AND) {
    head++;
    AST *right = parseEqExp();
    if ((left->type == AST::FLOAT || left->type == AST::INT) &&
        (right->type == AST::FLOAT || right->type == AST::INT)) {
      left->isFloat = false;
      left->iVal =
          (left->type == AST::FLOAT || right->type == AST::FLOAT)
              ? ((left->type == AST::FLOAT ? left->fVal : left->iVal) &&
                 (right->type == AST::FLOAT ? right->fVal : right->iVal))
              : (left->iVal && right->iVal);
      left->type = AST::INT;
      continue;
    }
    if ((left->type == AST::FLOAT && left->fVal) ||
        (left->type == AST::INT && left->iVal)) {
      left = right;
      continue;
    }
    if ((left->type == AST::FLOAT && !left->fVal) ||
        (left->type == AST::INT && !left->iVal)) {
      left->isFloat = false;
      left->type = AST::INT;
      left->iVal = 0;
      continue;
    }
    if ((right->type == AST::FLOAT && right->fVal) ||
        (right->type == AST::INT && right->iVal))
      continue;
    left = new AST(AST::L_AND_EXP, false, {left, right});
  }
  return left;
}

AST *SyntaxParser::parseLOrExp() {
  AST *left = parseLAndExp();
  while (tokens[head]->type == Token::L_OR) {
    head++;
    AST *right = parseLAndExp();
    if ((left->type == AST::FLOAT || left->type == AST::INT) &&
        (right->type == AST::FLOAT || right->type == AST::INT)) {
      left->isFloat = false;
      left->iVal =
          (left->type == AST::FLOAT || right->type == AST::FLOAT)
              ? ((left->type == AST::FLOAT ? left->fVal : left->iVal) ||
                 (right->type == AST::FLOAT ? right->fVal : right->iVal))
              : (left->iVal || right->iVal);
      left->type = AST::INT;
      continue;
    }
    if ((left->type == AST::FLOAT && left->fVal) ||
        (left->type == AST::INT && left->iVal)) {
      left->isFloat = false;
      left->type = AST::INT;
      left->iVal = 1;
      continue;
    }
    if ((left->type == AST::FLOAT && !left->fVal) ||
        (left->type == AST::INT && !left->iVal)) {
      left = right;
      continue;
    }
    if ((right->type == AST::FLOAT && !right->fVal) ||
        (right->type == AST::INT && !right->iVal))
      continue;
    left = new AST(AST::L_OR_EXP, false, {left, right});
  }
  return left;
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
    if (dimensions.back()->type == AST::INT)
      array.push_back(dimensions.back()->iVal);
    head++;
  }
  if (symbol->symbolType == Symbol::CONST &&
      array.size() == symbol->dimensions.size()) {
    if (symbol->dimensions.empty())
      return symbol->dataType == Symbol::FLOAT ? new AST(symbol->fVal)
                                               : new AST(symbol->iVal);
    int offset = 0;
    for (unsigned i = 0; i < array.size(); i++)
      offset = offset * symbol->dimensions[i] + array[i];
    if (symbol->dataType == Symbol::INT)
      return new AST(symbol->iMap.find(offset) == symbol->iMap.end()
                         ? 0
                         : symbol->iMap[offset]);
    else
      return new AST(symbol->fMap.find(offset) == symbol->fMap.end()
                         ? 0.0f
                         : symbol->fMap[offset]);
  }
  return new AST(AST::L_VAL, symbol->dataType == Symbol::FLOAT, symbol,
                 dimensions);
}

AST *SyntaxParser::parseMulExp() {
  AST *left = parseUnaryExp();
  while (tokens[head]->type == Token::DIV || tokens[head]->type == Token::MOD ||
         tokens[head]->type == Token::MUL) {
    AST::ASTType type = AST::ERR_TYPE;
    switch (tokens[head]->type) {
    case Token::DIV:
      type = AST::DIV_EXP;
      break;
    case Token::MOD:
      type = AST::MOD_EXP;
      break;
    case Token::MUL:
      type = AST::MUL_EXP;
      break;
    default:
      break;
    }
    head++;
    AST *right = parseUnaryExp();
    if ((left->type == AST::FLOAT || left->type == AST::INT) &&
        (right->type == AST::FLOAT || right->type == AST::INT)) {
      switch (type) {
      case AST::DIV_EXP:
        if (left->type == AST::FLOAT || right->type == AST::FLOAT) {
          left->isFloat = true;
          left->fVal = (left->type == AST::FLOAT ? left->fVal : left->iVal) /
                       (right->type == AST::FLOAT ? right->fVal : right->iVal);
          left->type = AST::FLOAT;
        } else
          left->iVal = left->iVal / right->iVal;
        break;
      case AST::MOD_EXP:
        left->iVal = left->iVal % right->iVal;
        break;
      case AST::MUL_EXP:
        if (left->type == AST::FLOAT || right->type == AST::FLOAT) {
          left->isFloat = true;
          left->fVal = (left->type == AST::FLOAT ? left->fVal : left->iVal) *
                       (right->type == AST::FLOAT ? right->fVal : right->iVal);
          left->type = AST::FLOAT;
        } else
          left->iVal = left->iVal * right->iVal;
        break;
      default:
        break;
      }
      continue;
    }
    if (!left->isFloat && right->isFloat)
      left = left->transIF();
    if (left->isFloat && !right->isFloat)
      right = right->transIF();
    left = new AST(type, left->isFloat, {left, right});
  }
  return left;
}

AST *SyntaxParser::parseRelExp() {
  AST *left = parseAddExp();
  while (tokens[head]->type == Token::GE || tokens[head]->type == Token::GT ||
         tokens[head]->type == Token::LE || tokens[head]->type == Token::LT) {
    AST::ASTType type = AST::ERR_TYPE;
    switch (tokens[head]->type) {
    case Token::GE:
      type = AST::GE_EXP;
      break;
    case Token::GT:
      type = AST::GT_EXP;
      break;
    case Token::LE:
      type = AST::LE_EXP;
      break;
    case Token::LT:
      type = AST::LT_EXP;
      break;
    default:
      break;
    }
    head++;
    AST *right = parseAddExp();
    if ((left->type == AST::FLOAT || left->type == AST::INT) &&
        (right->type == AST::FLOAT || right->type == AST::INT)) {
      switch (type) {
      case AST::GE_EXP:
        if (left->type == AST::FLOAT || right->type == AST::FLOAT) {
          left->isFloat = false;
          left->fVal = (left->type == AST::FLOAT ? left->fVal : left->iVal) >=
                       (right->type == AST::FLOAT ? right->fVal : right->iVal);
          left->type = AST::INT;
        } else
          left->iVal = left->iVal >= right->iVal;
        break;
      case AST::GT_EXP:
        if (left->type == AST::FLOAT || right->type == AST::FLOAT) {
          left->isFloat = false;
          left->fVal = (left->type == AST::FLOAT ? left->fVal : left->iVal) >
                       (right->type == AST::FLOAT ? right->fVal : right->iVal);
          left->type = AST::INT;
        } else
          left->iVal = left->iVal > right->iVal;
        break;
      case AST::LE_EXP:
        if (left->type == AST::FLOAT || right->type == AST::FLOAT) {
          left->isFloat = false;
          left->fVal = (left->type == AST::FLOAT ? left->fVal : left->iVal) <=
                       (right->type == AST::FLOAT ? right->fVal : right->iVal);
          left->type = AST::INT;
        } else
          left->iVal = left->iVal <= right->iVal;
        break;
      case AST::LT_EXP:
        if (left->type == AST::FLOAT || right->type == AST::FLOAT) {
          left->isFloat = false;
          left->fVal = (left->type == AST::FLOAT ? left->fVal : left->iVal) <
                       (right->type == AST::FLOAT ? right->fVal : right->iVal);
          left->type = AST::INT;
        } else
          left->iVal = left->iVal < right->iVal;
        break;
      default:
        break;
      }
      continue;
    }
    if (!left->isFloat && right->isFloat)
      left = left->transIF();
    if (left->isFloat && !right->isFloat)
      right = right->transIF();
    left = new AST(type, false, {left, right});
  }
  return left;
}

AST *SyntaxParser::parseReturnStmt(Symbol *func) {
  head++;
  if (tokens[head]->type == Token::SEMICOLON) {
    head++;
    return new AST(AST::RETURN_STMT, false, {});
  }
  AST *val = parseAddExp();
  head++;
  if ((func->dataType == Symbol::FLOAT) ^ val->isFloat)
    val = val->transIF();
  return new AST(AST::RETURN_STMT, false, {val});
}

void SyntaxParser::parseRoot() {
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
  root = new AST(AST::ROOT, false, items);
}

AST *SyntaxParser::parseStmt(Symbol *func) {
  switch (tokens[head]->type) {
  case Token::BREAK:
    head += 2;
    return new AST(AST::BREAK_STMT, false, {});
  case Token::CONTINUE:
    head += 2;
    return new AST(AST::CONTINUE_STMT, false, {});
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
    return new AST(AST::BLANK_STMT, false, {});
  case Token::WHILE:
    return parseWhileStmt(func);
  default:
    break;
  }
  return nullptr;
}

AST *SyntaxParser::parseUnaryExp() {
  switch (tokens[head]->type) {
  case Token::FLOAT_LIT:
    return new AST(tokens[head++]->fVal);
  case Token::ID:
    if (tokens[head + 1]->type == Token::LP)
      return parseFuncCall();
    return parseLVal();
  case Token::INT_LIT:
    return new AST(tokens[head++]->iVal);
  case Token::LP: {
    head++;
    AST *root = parseAddExp();
    head++;
    return root;
  }
  case Token::L_NOT: {
    head++;
    AST *val = parseUnaryExp();
    switch (val->type) {
    case AST::FLOAT:
      val->isFloat = false;
      val->type = AST::INT;
      val->iVal = !val->fVal;
      return val;
    case AST::INT:
      val->iVal = !val->iVal;
      return val;
    case AST::L_NOT_EXP:
      if (val->nodes[0]->type == AST::L_NOT_EXP) {
        AST *ret = val->nodes[0];
        val->nodes.clear();
        return ret;
      }
      break;
    case AST::NEG_EXP:
      return val;
    default:
      break;
    }
    return new AST(AST::L_NOT_EXP, false, {val});
  }
  case Token::MINUS: {
    head++;
    AST *val = parseUnaryExp();
    switch (val->type) {
    case AST::FLOAT:
      val->fVal = -val->fVal;
      return val;
    case AST::INT:
      val->iVal = -val->iVal;
      return val;
    case AST::NEG_EXP: {
      AST *ret = val->nodes[0];
      val->nodes.clear();
      return ret;
    }
    default:
      break;
    }
    return new AST(AST::NEG_EXP, val->isFloat, {val});
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
      head++;
    }
    Symbol *symbol = new Symbol(Symbol::LOCAL_VAR, type, name, dimensions);
    items.push_back(new AST(AST::LOCAL_VAR_DEF, false, symbol, {}));
    if (tokens[head]->type == Token::ASSIGN) {
      head++;
      AST *val = parseInitVal();
      if (dimensions.empty()) {
        if ((type == Symbol::FLOAT) ^ val->isFloat)
          val = val->transIF();
        items.push_back(new AST(
            AST::ASSIGN_STMT, false,
            {new AST(AST::L_VAL, type == Symbol::FLOAT, symbol, {}), val}));
      } else {
        unordered_map<int, AST *> exps;
        allocInitVal(dimensions, exps, 0, val);
        vector<pair<int, AST *>> orderedExps(exps.begin(), exps.end());
        sort(orderedExps.begin(), orderedExps.end());
        unsigned totalSize = 1;
        for (unsigned dimension : dimensions)
          totalSize *= dimension;
        if (totalSize > 1024 && orderedExps.size() <= totalSize / 2) {
          items.push_back(new AST(AST::MEMSET_ZERO, false, symbol, {}));
          for (pair<int, AST *> exp : orderedExps) {
            vector<AST *> dimensionASTs(dimensions.size());
            unsigned t = exp.first;
            for (int j = dimensions.size() - 1; j >= 0; j--) {
              dimensionASTs[j] = new AST((int)t % dimensions[j]);
              t /= dimensions[j];
            }
            AST *expVal = exp.second;
            if ((type == Symbol::FLOAT) ^ expVal->isFloat)
              expVal = expVal->transIF();
            items.push_back(new AST(AST::ASSIGN_STMT, false,
                                    {new AST(AST::L_VAL, type == Symbol::FLOAT,
                                             symbol, dimensionASTs),
                                     expVal}));
          }
        } else {
          for (unsigned i = 0; i < totalSize; i++) {
            vector<AST *> dimensionASTs(dimensions.size());
            unsigned t = i;
            for (int j = dimensions.size() - 1; j >= 0; j--) {
              dimensionASTs[j] = new AST((int)t % dimensions[j]);
              t /= dimensions[j];
            }
            if (exps.find(i) == exps.end()) {
              items.push_back(
                  new AST(AST::ASSIGN_STMT, false,
                          {new AST(AST::L_VAL, false, symbol, dimensionASTs),
                           new AST(0)}));
            } else {
              AST *expVal = exps[i];
              if ((type == Symbol::FLOAT) ^ expVal->isFloat)
                expVal = expVal->transIF();
              items.push_back(
                  new AST(AST::ASSIGN_STMT, false,
                          {new AST(AST::L_VAL, type == Symbol::FLOAT, symbol,
                                   dimensionASTs),
                           expVal}));
            }
          }
        }
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
  if ((cond->type == AST::FLOAT && !cond->fVal) ||
      (cond->type == AST::INT && !cond->iVal))
    return new AST(AST::BLANK_STMT, false, {});
  return new AST(AST::WHILE_STMT, false, {cond, body});
}
