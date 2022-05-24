#include <iostream>
#include <string.h>

#include "AST.h"
#include "GrammarParser.h"

using namespace std;

GrammarParser::GrammarParser(const vector<Token *> tokens) {
  this->rootAST = nullptr;
  this->isProcessed = false;
  this->head = 0;
  this->tokens = tokens;
  this->depth = 0;
}

GrammarParser::~GrammarParser() {
  if (isProcessed)
    delete rootAST;
}

Symbol *GrammarParser::lastSymbol(string name) {
  int index = symbolStack.size() - 1;
  while (index >= 0 && name.compare(symbolStack[index]->name))
    index--;
  if (index < 0)
    return nullptr;
  return symbolStack[index];
}

AST *GrammarParser::parseAddExp() {
  vector<AST *> items;
  vector<Type> ops;
  items.push_back(parseMulExp());
  while (tokens[head]->type == MINUS || tokens[head]->type == PLUS) {
    ops.push_back(tokens[head]->type);
    head++;
    items.push_back(parseMulExp());
  }
  while (!ops.empty()) {
    bool gotVal1 = false;
    bool isInt1 = false;
    bool gotVal2 = false;
    bool isInt2 = false;
    int intVal1 = 0;
    float floatVal1 = 0;
    int intVal2 = 0;
    float floatVal2 = 0;
    switch (items[0]->astType) {
    case FLOAT_LITERAL:
      gotVal1 = true;
      isInt1 = false;
      floatVal1 = items[0]->fVal;
      break;
    case INT_LITERAL:
      gotVal1 = true;
      isInt1 = true;
      intVal1 = items[0]->iVal;
      break;
    default:
      break;
    }
    switch (items[1]->astType) {
    case FLOAT_LITERAL:
      gotVal2 = true;
      isInt2 = false;
      floatVal2 = items[1]->fVal;
      break;
    case INT_LITERAL:
      gotVal2 = true;
      isInt2 = true;
      intVal2 = items[1]->iVal;
      break;
    default:
      break;
    }
    if (gotVal1 && gotVal2) {
      Type op = ops.front();
      delete items[0];
      delete items[1];
      items.erase(items.begin() + 1);
      ops.erase(ops.begin());
      switch (op) {
      case MINUS:
        items[0] = isInt1 && isInt2 ? new AST(intVal1 - intVal2)
                                    : new AST((isInt1 ? intVal1 : floatVal1) -
                                              (isInt2 ? intVal2 : floatVal2));
        break;
      case PLUS:
        items[0] = isInt1 && isInt2 ? new AST(intVal1 + intVal2)
                                    : new AST((isInt1 ? intVal1 : floatVal1) +
                                              (isInt2 ? intVal2 : floatVal2));
        break;
      default:
        break;
      }
      continue;
    }
    break;
  }
  if (items.size() == 1) {
    AST *ret = items[0];
    items[0] = nullptr;
    return ret;
  }
  for (unsigned i = 0; i < ops.size(); i++) {
    if (items[i + 1]->astType == UNARY_EXP && items[i + 1]->type == MINUS) {
      AST *val = items[i + 1]->nodes.front();
      items[i + 1]->nodes.front() = nullptr;
      items[i + 1] = val;
      switch (ops[i]) {
      case MINUS:
        ops[i] = PLUS;
        break;
      case PLUS:
        ops[i] = MINUS;
        break;
      default:
        break;
      }
    }
  }
  return new AST(BINARY_EXP, items, ops);
}

AST *GrammarParser::parseAssignStmt() {
  AST *lVal = parseLVal();
  head++;
  AST *rVal = parseAddExp();
  head++;
  return new AST(ASSIGN_STMT, lVal, rVal);
}

AST *GrammarParser::parseBlock() {
  vector<AST *> items;
  head++;
  while (tokens[head]->type != RC) {
    switch (tokens[head]->type) {
    case BREAK:
    case CONTINUE:
    case ID:
    case IF:
    case LC:
    case RETURN:
    case SEMICOLON:
    case WHILE:
      items.push_back(parseStmt());
      if (!items.back())
        items.pop_back();
      break;
    case CONST: {
      vector<AST *> consts = parseConstDef();
      items.insert(items.end(), consts.begin(), consts.end());
      break;
    }
    case FLOAT:
    case INT: {
      vector<AST *> vars = parseVarDef();
      items.insert(items.end(), vars.begin(), vars.end());
      break;
    }
    default:
      break;
    }
  }
  head++;
  return new AST(BLOCK, items);
}

vector<AST *> GrammarParser::parseConstDef() {
  head++;
  Type type = tokens[head]->type;
  head++;
  vector<AST *> consts;
  while (tokens[head]->type != SEMICOLON) {
    Symbol *symbol = new Symbol();
    symbols.push_back(symbol);
    symbolStack.push_back(symbol);
    symbol->symbolType = CONST;
    symbol->depth = depth;
    symbol->dataType = type;
    symbol->name = tokens[head]->stringValue;
    head++;
    bool isArray = false;
    while (tokens[head]->type == LB) {
      isArray = true;
      head++;
      AST *val = parseAddExp();
      symbol->dimensions.push_back(val->iVal);
      delete val;
      head++;
    }
    int size = 1;
    for (int dimension : symbol->dimensions)
      size *= dimension;
    head++;
    AST *val = parseInitVal();
    if (isArray)
      switch (symbol->dataType) {
      case FLOAT:
        symbol->floatArray = new float[size];
        memset(symbol->floatArray, 0, sizeof(float) * size);
        parseConstInitVal(symbol->dimensions, symbol->floatArray, val);
        break;
      case INT:
        symbol->intArray = new int[size];
        memset(symbol->intArray, 0, sizeof(int) * size);
        parseConstInitVal(symbol->dimensions, symbol->intArray, val);
        break;
      default:
        break;
      }
    else
      switch (symbol->dataType) {
      case FLOAT:
        switch (val->astType) {
        case FLOAT_LITERAL:
          symbol->floatValue = val->fVal;
          break;
        case INT_LITERAL:
          symbol->floatValue = val->iVal;
          break;
        default:
          break;
        }
        break;
      case INT:
        switch (val->astType) {
        case FLOAT_LITERAL:
          symbol->intValue = val->fVal;
          break;
        case INT_LITERAL:
          symbol->intValue = val->iVal;
          break;
        default:
          break;
        }
        break;
      default:
        break;
      }
    delete val;
    consts.push_back(new AST(CONST_DEF, symbol));
    if (tokens[head]->type == SEMICOLON)
      break;
    head++;
  }
  head++;
  return consts;
}

template <typename T>
void GrammarParser::parseConstInitVal(vector<int> array, T *dst, AST *src) {
  if (array.empty()) {
    while (src && src->astType == INIT_VAL) {
      if (src->nodes.empty())
        src = nullptr;
      else
        src = src->nodes.front();
    }
    if (src) {
      switch (src->astType) {
      case FLOAT_LITERAL:
        *dst = src->fVal;
        break;
      case INT_LITERAL:
        *dst = src->iVal;
        break;
      default:
        break;
      }
    }
    return;
  }
  vector<int> index(array.size(), 0);
  vector<AST *> stk(src->nodes.rbegin(), src->nodes.rend());
  while (!stk.empty()) {
    if (stk.back()->astType != INIT_VAL) {
      int offset = 0;
      for (unsigned i = 0; i < array.size(); i++)
        offset = offset * array[i] + index[i];
      switch (stk.back()->astType) {
      case FLOAT_LITERAL:
        dst[offset] = stk.back()->fVal;
        break;
      case INT_LITERAL:
        dst[offset] = stk.back()->iVal;
        break;
      default:
        break;
      }
      index.back()++;
    } else {
      unsigned d = array.size() - 1;
      while (d > 0 && index[d] == 0)
        d--;
      vector<int> newArray;
      for (unsigned i = d + 1; i < array.size(); i++)
        newArray.push_back(array[i]);
      int offset = 0;
      for (unsigned i = 0; i < array.size(); i++)
        offset = offset * array[i] + (i <= d ? index[i] : 0);
      parseConstInitVal(newArray, dst + offset, stk.back());
      index[d]++;
    }
    for (int i = array.size() - 1; i >= 0; i--) {
      if (index[i] < array[i])
        break;
      index[i] = 0;
      if (i == 0)
        return;
      index[i - 1]++;
    }
    stk.pop_back();
  }
}

AST *GrammarParser::parseEqExp() {
  vector<AST *> items;
  vector<Type> ops;
  items.push_back(parseRelExp());
  while (tokens[head]->type == EQ || tokens[head]->type == NE) {
    ops.push_back(tokens[head]->type);
    head++;
    items.push_back(parseRelExp());
  }
  while (!ops.empty()) {
    bool gotVal1 = false;
    bool isInt1 = false;
    bool gotVal2 = false;
    bool isInt2 = false;
    int intVal1 = 0;
    float floatVal1 = 0;
    int intVal2 = 0;
    float floatVal2 = 0;
    switch (items[0]->astType) {
    case FLOAT_LITERAL:
      gotVal1 = true;
      isInt1 = false;
      floatVal1 = items[0]->fVal;
      break;
    case INT_LITERAL:
      gotVal1 = true;
      isInt1 = true;
      intVal1 = items[0]->iVal;
      break;
    default:
      break;
    }
    switch (items[1]->astType) {
    case FLOAT_LITERAL:
      gotVal2 = true;
      isInt2 = false;
      floatVal2 = items[1]->fVal;
      break;
    case INT_LITERAL:
      gotVal2 = true;
      isInt2 = true;
      intVal2 = items[1]->iVal;
      break;
    default:
      break;
    }
    if (gotVal1 && gotVal2) {
      Type op = ops.front();
      delete items[0];
      delete items[1];
      items.erase(items.begin() + 1);
      ops.erase(ops.begin());
      switch (op) {
      case EQ:
        items[0] = isInt1 && isInt2 ? new AST(intVal1 == intVal2)
                                    : new AST((isInt1 ? intVal1 : floatVal1) ==
                                              (isInt2 ? intVal2 : floatVal2));
        break;
      case NE:
        items[0] = isInt1 && isInt2 ? new AST(intVal1 != intVal2)
                                    : new AST((isInt1 ? intVal1 : floatVal1) !=
                                              (isInt2 ? intVal2 : floatVal2));
        break;
      default:
        break;
      }
      continue;
    }
    break;
  }
  if (items.size() == 1) {
    AST *ret = items[0];
    items[0] = nullptr;
    return ret;
  }
  return new AST(BINARY_EXP, items, ops);
}

AST *GrammarParser::parseExpStmt() {
  AST *exp = parseAddExp();
  head++;
  return new AST(EXP_STMT, exp);
}

AST *GrammarParser::parseFuncCall() {
  string func = tokens[head]->stringValue;
  head++;
  head++;
  vector<AST *> params;
  while (tokens[head]->type != RP) {
    params.push_back(parseAddExp());
    if (tokens[head]->type == RP)
      break;
    head++;
  }
  head++;
  Symbol *symbol = lastSymbol(func);
  if (symbol)
    return new AST(FUNC_CALL, symbol, params);
  else
    return new AST(FUNC_CALL, func, params);
}

AST *GrammarParser::parseFuncDef() {
  Symbol *symbol = new Symbol();
  symbols.push_back(symbol);
  symbolStack.push_back(symbol);
  symbol->symbolType = FUNC;
  symbol->depth = depth;
  symbol->dataType = tokens[head]->type;
  head++;
  symbol->name = tokens[head]->stringValue;
  head++;
  head++;
  depth++;
  vector<AST *> params;
  while (tokens[head]->type != RP) {
    params.push_back(parseFuncFParam());
    symbol->params.push_back(symbolStack.back());
    if (tokens[head]->type == RP)
      break;
    head++;
  }
  head++;
  AST *body = parseBlock();
  depth--;
  while (!symbolStack.empty() && symbolStack.back()->depth > depth)
    symbolStack.pop_back();
  return new AST(FUNC_DEF, symbol->dataType, symbol->name, params, body);
}

AST *GrammarParser::parseFuncFParam() {
  Symbol *symbol = new Symbol();
  symbols.push_back(symbol);
  symbolStack.push_back(symbol);
  symbol->symbolType = PARAM;
  symbol->depth = depth;
  symbol->dataType = tokens[head]->type;
  head++;
  symbol->name = tokens[head]->stringValue;
  head++;
  if (tokens[head]->type != LB)
    return new AST(FUNC_PARAM, symbol->dataType, symbol->name);
  symbol->dimensions.push_back(-1);
  vector<AST *> dimensions;
  dimensions.push_back(nullptr);
  head++;
  head++;
  while (tokens[head]->type == LB) {
    head++;
    dimensions.push_back(parseAddExp());
    symbol->dimensions.push_back(dimensions.back()->iVal);
    head++;
  }
  return new AST(FUNC_PARAM, symbol->dataType, symbol->name, dimensions);
}

AST *GrammarParser::parseIfStmt() {
  head++;
  head++;
  AST *cond = parseLOrExp();
  head++;
  bool depthInc = tokens[head]->type != LC;
  if (depthInc)
    depth++;
  AST *stmt1 = parseStmt();
  if (depthInc)
    depth--;
  while (!symbolStack.empty() && symbolStack.back()->depth > depth)
    symbolStack.pop_back();
  AST *stmt2 = nullptr;
  if (tokens[head]->type == ELSE) {
    depthInc = tokens[head]->type != LC;
    head++;
    if (depthInc)
      depth++;
    stmt2 = parseStmt();
    if (depthInc)
      depth--;
    while (!symbolStack.empty() && symbolStack.back()->depth > depth)
      symbolStack.pop_back();
  }
  return new AST(IF_STMT, cond, stmt1, stmt2);
}

AST *GrammarParser::parseInitVal() {
  if (tokens[head]->type != LC)
    return parseAddExp();
  head++;
  vector<AST *> items;
  while (tokens[head]->type != RC) {
    items.push_back(parseInitVal());
    if (tokens[head]->type == RC)
      break;
    head++;
  }
  head++;
  return new AST(INIT_VAL, items);
}

AST *GrammarParser::parseLAndExp() {
  vector<AST *> items;
  items.push_back(parseEqExp());
  while (tokens[head]->type == L_AND) {
    head++;
    items.push_back(parseEqExp());
  }
  while (items.size() > 1) {
    bool gotVal1 = false;
    bool isInt1 = false;
    bool gotVal2 = false;
    bool isInt2 = false;
    int intVal1 = 0;
    float floatVal1 = 0;
    int intVal2 = 0;
    float floatVal2 = 0;
    switch (items[0]->astType) {
    case FLOAT_LITERAL:
      gotVal1 = true;
      isInt1 = false;
      floatVal1 = items[0]->fVal;
      break;
    case INT_LITERAL:
      gotVal1 = true;
      isInt1 = true;
      intVal1 = items[0]->iVal;
      break;
    default:
      break;
    }
    switch (items[1]->astType) {
    case FLOAT_LITERAL:
      gotVal2 = true;
      isInt2 = false;
      floatVal2 = items[1]->fVal;
      break;
    case INT_LITERAL:
      gotVal2 = true;
      isInt2 = true;
      intVal2 = items[1]->iVal;
      break;
    default:
      break;
    }
    if (gotVal1 && gotVal2) {
      delete items[0];
      delete items[1];
      items.erase(items.begin() + 1);
      items[0] = new AST((isInt1 ? intVal1 : floatVal1) &&
                         (isInt2 ? intVal2 : floatVal2));
      continue;
    }
    break;
  }
  if (items.size() == 1) {
    AST *ret = items[0];
    items[0] = nullptr;
    return ret;
  }
  return new AST(BINARY_EXP, L_AND, items);
}

AST *GrammarParser::parseLOrExp() {
  vector<AST *> items;
  items.push_back(parseLAndExp());
  while (tokens[head]->type == L_OR) {
    head++;
    items.push_back(parseLAndExp());
  }
  while (items.size() > 1) {
    bool gotVal1 = false;
    bool isInt1 = false;
    bool gotVal2 = false;
    bool isInt2 = false;
    int intVal1 = 0;
    float floatVal1 = 0;
    int intVal2 = 0;
    float floatVal2 = 0;
    switch (items[0]->astType) {
    case FLOAT_LITERAL:
      gotVal1 = true;
      isInt1 = false;
      floatVal1 = items[0]->fVal;
      break;
    case INT_LITERAL:
      gotVal1 = true;
      isInt1 = true;
      intVal1 = items[0]->iVal;
      break;
    default:
      break;
    }
    switch (items[1]->astType) {
    case FLOAT_LITERAL:
      gotVal2 = true;
      isInt2 = false;
      floatVal2 = items[1]->fVal;
      break;
    case INT_LITERAL:
      gotVal2 = true;
      isInt2 = true;
      intVal2 = items[1]->iVal;
      break;
    default:
      break;
    }
    if (gotVal1 && gotVal2) {
      delete items[0];
      delete items[1];
      items.erase(items.begin() + 1);
      items[0] = new AST((isInt1 ? intVal1 : floatVal1) ||
                         (isInt2 ? intVal2 : floatVal2));
      continue;
    }
    break;
  }
  if (items.size() == 1) {
    AST *ret = items[0];
    items[0] = nullptr;
    return ret;
  }
  return new AST(BINARY_EXP, L_OR, items);
}

AST *GrammarParser::parseLVal() {
  bool toBeValue = true;
  vector<int> array;
  string name = tokens[head]->stringValue;
  Symbol *symbol = lastSymbol(name);
  if (symbol->symbolType != CONST)
    toBeValue = false;
  head++;
  vector<AST *> dimensions;
  while (tokens[head]->type == LB) {
    head++;
    dimensions.push_back(parseAddExp());
    switch (dimensions.back()->astType) {
    case FLOAT_LITERAL:
      array.push_back(dimensions.back()->fVal);
      break;
    case INT_LITERAL:
      array.push_back(dimensions.back()->iVal);
      break;
    default:
      toBeValue = false;
      break;
    }
    head++;
  }
  if (array.size() != symbol->dimensions.size())
    toBeValue = false;
  if (toBeValue) {
    if (symbol->dimensions.empty()) {
      switch (symbol->dataType) {
      case FLOAT:
        return new AST(symbol->floatValue);
      case INT:
        return new AST(symbol->intValue);
      default:
        break;
      }
    } else {
      int offset = 0;
      for (unsigned i = 0; i < array.size(); i++)
        offset = offset * symbol->dimensions[i] + array[i];
      switch (symbol->dataType) {
      case FLOAT:
        return new AST(symbol->floatArray[offset]);
      case INT:
        return new AST(symbol->intArray[offset]);
      default:
        break;
      }
    }
  }
  return new AST(L_VAL, symbol, dimensions);
}

AST *GrammarParser::parseMulExp() {
  vector<AST *> items;
  vector<Type> ops;
  items.push_back(parseUnaryExp());
  while (tokens[head]->type == DIV || tokens[head]->type == MOD ||
         tokens[head]->type == MUL) {
    ops.push_back(tokens[head]->type);
    head++;
    items.push_back(parseUnaryExp());
  }
  while (!ops.empty()) {
    bool gotVal1 = false;
    bool isInt1 = false;
    bool gotVal2 = false;
    bool isInt2 = false;
    int intVal1 = 0;
    float floatVal1 = 0;
    int intVal2 = 0;
    float floatVal2 = 0;
    switch (items[0]->astType) {
    case FLOAT_LITERAL:
      gotVal1 = true;
      isInt1 = false;
      floatVal1 = items[0]->fVal;
      break;
    case INT_LITERAL:
      gotVal1 = true;
      isInt1 = true;
      intVal1 = items[0]->iVal;
      break;
    default:
      break;
    }
    switch (items[1]->astType) {
    case FLOAT_LITERAL:
      gotVal2 = true;
      isInt2 = false;
      floatVal2 = items[1]->fVal;
      break;
    case INT_LITERAL:
      gotVal2 = true;
      isInt2 = true;
      intVal2 = items[1]->iVal;
      break;
    default:
      break;
    }
    if (gotVal1 && gotVal2) {
      Type op = ops.front();
      delete items[0];
      delete items[1];
      items.erase(items.begin() + 1);
      ops.erase(ops.begin());
      switch (op) {
      case DIV:
        items[0] = isInt1 && isInt2 ? new AST(intVal1 / intVal2)
                                    : new AST((isInt1 ? intVal1 : floatVal1) /
                                              (isInt2 ? intVal2 : floatVal2));
        break;
      case MOD:
        if (isInt1 && isInt2)
          items[0] = new AST(intVal1 % intVal2);
        break;
      case MUL:
        items[0] = isInt1 && isInt2 ? new AST(intVal1 * intVal2)
                                    : new AST((isInt1 ? intVal1 : floatVal1) *
                                              (isInt2 ? intVal2 : floatVal2));
        break;
      default:
        break;
      }
      continue;
    }
    break;
  }
  if (items.size() == 1) {
    AST *ret = items[0];
    items[0] = nullptr;
    return ret;
  }
  return new AST(BINARY_EXP, items, ops);
}

AST *GrammarParser::parseRelExp() {
  vector<AST *> items;
  vector<Type> ops;
  items.push_back(parseAddExp());
  while (tokens[head]->type == GE || tokens[head]->type == GT ||
         tokens[head]->type == LE || tokens[head]->type == LT) {
    ops.push_back(tokens[head]->type);
    head++;
    items.push_back(parseAddExp());
  }
  while (!ops.empty()) {
    bool gotVal1 = false;
    bool isInt1 = false;
    bool gotVal2 = false;
    bool isInt2 = false;
    int intVal1 = 0;
    float floatVal1 = 0;
    int intVal2 = 0;
    float floatVal2 = 0;
    switch (items[0]->astType) {
    case FLOAT_LITERAL:
      gotVal1 = true;
      isInt1 = false;
      floatVal1 = items[0]->fVal;
      break;
    case INT_LITERAL:
      gotVal1 = true;
      isInt1 = true;
      intVal1 = items[0]->iVal;
      break;
    default:
      break;
    }
    switch (items[1]->astType) {
    case FLOAT_LITERAL:
      gotVal2 = true;
      isInt2 = false;
      floatVal2 = items[1]->fVal;
      break;
    case INT_LITERAL:
      gotVal2 = true;
      isInt2 = true;
      intVal2 = items[1]->iVal;
      break;
    default:
      break;
    }
    if (gotVal1 && gotVal2) {
      Type op = ops.front();
      delete items[0];
      delete items[1];
      items.erase(items.begin() + 1);
      ops.erase(ops.begin());
      switch (op) {
      case GE:
        items[0] = new AST((isInt1 ? intVal1 : floatVal1) >=
                           (isInt2 ? intVal2 : floatVal2));
        break;
      case GT:
        items[0] = new AST((isInt1 ? intVal1 : floatVal1) >
                           (isInt2 ? intVal2 : floatVal2));
        break;
      case LE:
        items[0] = new AST((isInt1 ? intVal1 : floatVal1) <=
                           (isInt2 ? intVal2 : floatVal2));
        break;
      case LT:
        items[0] = new AST((isInt1 ? intVal1 : floatVal1) <
                           (isInt2 ? intVal2 : floatVal2));
        break;
      default:
        break;
      }
      continue;
    }
    break;
  }
  if (items.size() == 1) {
    AST *ret = items[0];
    items[0] = nullptr;
    return ret;
  }
  return new AST(BINARY_EXP, items, ops);
}

AST *GrammarParser::parseReturnStmt() {
  head++;
  if (tokens[head]->type == SEMICOLON) {
    head++;
    return new AST(RETURN_STMT);
  }
  AST *val = parseAddExp();
  head++;
  return new AST(RETURN_STMT, val);
}

void GrammarParser::parseRoot() {
  if (isProcessed)
    return;
  isProcessed = true;
  vector<AST *> items;
  while (head < tokens.size()) {
    switch (tokens[head]->type) {
    case CONST: {
      vector<AST *> consts = parseConstDef();
      items.insert(items.end(), consts.begin(), consts.end());
      break;
    }
    case FLOAT:
    case INT:
      switch (tokens[head + 2]->type) {
      case ASSIGN:
      case COMMA:
      case LB:
      case SEMICOLON: {
        vector<AST *> vars = parseVarDef();
        items.insert(items.end(), vars.begin(), vars.end());
        break;
      }
      case LP:
        items.push_back(parseFuncDef());
        break;
      default:
        break;
      }
      break;
    case VOID:
      items.push_back(parseFuncDef());
      break;
    default:
      break;
    }
  }
  rootAST = new AST(ROOT, items);
}

AST *GrammarParser::parseStmt() {
  switch (tokens[head]->type) {
  case BREAK: {
    head++;
    head++;
    return new AST(BREAK_STMT);
  }
  case CONTINUE: {
    head++;
    head++;
    return new AST(CONTINUE_STMT);
  }
  case ID:
    switch (tokens[head + 1]->type) {
    case ASSIGN:
    case LB:
      return parseAssignStmt();
    case DIV:
    case LP:
    case MINUS:
    case MOD:
    case MUL:
    case PLUS:
      return parseExpStmt();
    default:
      break;
    }
  case IF:
    return parseIfStmt();
  case LC: {
    depth++;
    AST *root = parseBlock();
    depth--;
    while (!symbolStack.empty() && symbolStack.back()->depth > depth)
      symbolStack.pop_back();
    return root;
  }
  case LP:
  case PLUS:
  case MINUS:
  case L_NOT:
    return parseExpStmt();
  case RETURN:
    return parseReturnStmt();
  case SEMICOLON:
    head++;
    return new AST(BLANK_STMT);
  case WHILE:
    return parseWhileStmt();
  default:
    break;
  }
  return nullptr;
}

AST *GrammarParser::parseUnaryExp() {
  switch (tokens[head]->type) {
  case FLOAT_LITERAL: {
    float fVal = tokens[head]->floatValue;
    head++;
    return new AST(fVal);
  }
  case ID:
    if (tokens[head + 1]->type == LP)
      return parseFuncCall();
    else
      return parseLVal();
  case INT_LITERAL: {
    int iVal = tokens[head]->intValue;
    head++;
    return new AST(iVal);
  }
  case LP: {
    head++;
    AST *root = parseAddExp();
    head++;
    return root;
  }
  case L_NOT: {
    head++;
    AST *val = parseUnaryExp();
    switch (val->astType) {
    case FLOAT_LITERAL: {
      float fVal = val->fVal;
      delete val;
      return new AST(!fVal);
    }
    case INT_LITERAL: {
      int iVal = val->iVal;
      delete val;
      return new AST(!iVal);
    }
    case UNARY_EXP:
      switch (val->type) {
      case L_NOT: {
        AST *ret = val->nodes.front();
        val->nodes.front() = nullptr;
        delete val;
        return ret;
      }
      case MINUS:
        return val;
      default:
        break;
      }
      break;
    default:
      break;
    }
    return new AST(UNARY_EXP, L_NOT, val);
  }
  case MINUS: {
    head++;
    AST *val = parseUnaryExp();
    switch (val->astType) {
    case FLOAT_LITERAL:
      val->fVal = -val->fVal;
      return val;
    case INT_LITERAL:
      val->iVal = -val->iVal;
      return val;
    case UNARY_EXP:
      if (val->type == MINUS) {
        AST *ret = val->nodes.front();
        val->nodes.front() = nullptr;
        delete val;
        return ret;
      }
      break;
    default:
      break;
    }
    return new AST(UNARY_EXP, MINUS, val);
  }
  case PLUS: {
    head++;
    return parseUnaryExp();
  }
  default:
    break;
  }
  return nullptr;
}

vector<AST *> GrammarParser::parseVarDef() {
  vector<AST *> vars;
  Type type = tokens[head]->type;
  head++;
  while (tokens[head]->type != SEMICOLON) {
    Symbol *symbol = new Symbol();
    symbols.push_back(symbol);
    symbolStack.push_back(symbol);
    symbol->symbolType = VAR;
    symbol->depth = depth;
    symbol->dataType = type;
    symbol->name = tokens[head]->stringValue;
    head++;
    while (tokens[head]->type == LB) {
      head++;
      AST *val = parseAddExp();
      symbol->dimensions.push_back(val->iVal);
      delete val;
      head++;
    }
    AST *val = nullptr;
    if (tokens[head]->type == ASSIGN) {
      head++;
      val = parseInitVal();
    }
    vars.push_back(val == nullptr ? new AST(VAR_DEF, symbol)
                                  : new AST(VAR_DEF, symbol, val));
    if (tokens[head]->type == SEMICOLON)
      break;
    head++;
  }
  head++;
  return vars;
}

AST *GrammarParser::parseWhileStmt() {
  head++;
  head++;
  AST *cond = parseLOrExp();
  head++;
  bool depthInc = tokens[head]->type != LC;
  if (depthInc)
    depth++;
  AST *body = parseStmt();
  if (depthInc)
    depth--;
  while (!symbolStack.empty() && symbolStack.back()->depth > depth)
    symbolStack.pop_back();
  return new AST(WHILE_STMT, cond, body);
}

AST *GrammarParser::getAST() {
  if (!isProcessed)
    parseRoot();
  return rootAST;
};

vector<Symbol *> &GrammarParser::getSymbolTable() {
  if (!isProcessed)
    parseRoot();
  return symbols;
}