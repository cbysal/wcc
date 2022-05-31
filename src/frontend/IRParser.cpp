#include <algorithm>
#include <iostream>
#include <queue>
#include <utility>

#include "IRParser.h"

using namespace std;

IRParser::IRParser(AST *root, vector<Symbol *> symbols) {
  this->isProcessed = false;
  this->tempId = 0;
  this->root = root;
  this->symbols = symbols;
}

IRParser::~IRParser() {}

vector<IR *> IRParser::parseAddExp(AST *root) {
  vector<IR *> irs1 = parseAST(root->nodes[0]);
  vector<IR *> irs2 = parseAST(root->nodes[1]);
  int t1 = irs1.back()->items.front()->tempId;
  int t2 = irs2.back()->items.front()->tempId;
  int t3 = tempId++;
  vector<IR *> irs3;
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  switch (root->type) {
  case MINUS:
    irs3.push_back(new IR(SUB, {new IRItem(TEMP, t3), new IRItem(TEMP, t1),
                                new IRItem(TEMP, t2)}));
    break;
  case PLUS:
    irs3.push_back(new IR(ADD, {new IRItem(TEMP, t3), new IRItem(TEMP, t1),
                                new IRItem(TEMP, t2)}));
    break;
  default:
    break;
  }
  return irs3;
}

vector<IR *> IRParser::parseAssignStmt(AST *root) {
  vector<IR *> irs1 = parseLVal(root->nodes[0]);
  vector<IR *> irs2 = parseAST(root->nodes[1]);
  vector<IR *> irs3;
  int t1 = irs1.back()->items.front()->tempId;
  int t2 = irs2.back()->items.front()->tempId;
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.push_back(new IR(STORE, {new IRItem(TEMP, t1), new IRItem(TEMP, t2)}));
  return irs3;
}

vector<IR *> IRParser::parseAST(AST *root) {
  switch (root->astType) {
  case ASSIGN_STMT:
    return parseAssignStmt(root);
  case BINARY_EXP:
    return parseBinaryExp(root);
  case BLANK_STMT:
    return vector<IR *>();
  case BLOCK:
    return parseBlock(root);
  case BREAK_STMT:
    return {new IR(BREAK)};
  case CONTINUE_STMT:
    return {new IR(CONTINUE)};
  case EXP_STMT:
    return parseAST(root->nodes[0]);
  case FLOAT_LITERAL:
    return parseFloatLiteral(root);
  case FUNC_CALL:
    return parseFuncCall(root);
  case IF_STMT:
    return parseIfStmt(root);
  case INT_LITERAL:
    return parseIntLiteral(root);
  case L_VAL: {
    vector<IR *> irs = parseLVal(root);
    irs.push_back(
        new IR(LOAD, {new IRItem(TEMP, tempId++),
                      new IRItem(TEMP, irs.back()->items.front()->tempId)}));
    return irs;
  }
  case RETURN_STMT:
    return parseReturnStmt(root);
  case UNARY_EXP:
    return parseUnaryExp(root);
  case WHILE_STMT:
    return parseWhileStmt(root);
  default:
    break;
  }
  return vector<IR *>();
}

vector<IR *> IRParser::parseBinaryExp(AST *root) {
  switch (root->type) {
  case DIV:
  case MOD:
  case MUL:
    return parseMulExp(root);
  case EQ:
  case NE:
    return parseEqExp(root);
  case GE:
  case GT:
  case LE:
  case LT:
    return parseRelExp(root);
  case L_AND:
    return parseLAndExp(root);
  case L_OR:
    return parseLOrExp(root);
  case MINUS:
  case PLUS:
    return parseAddExp(root);
  default:
    break;
  }
  return vector<IR *>();
}

vector<IR *> IRParser::parseBlock(AST *root) {
  vector<IR *> irs;
  for (AST *node : root->nodes) {
    vector<IR *> moreIRs = parseAST(node);
    irs.insert(irs.end(), moreIRs.begin(), moreIRs.end());
  }
  return irs;
}

vector<IR *> IRParser::parseEqExp(AST *root) {
  vector<IR *> irs1 = parseAST(root->nodes[0]);
  vector<IR *> irs2 = parseAST(root->nodes[1]);
  int t1 = irs1.back()->items.front()->tempId;
  int t2 = irs2.back()->items.front()->tempId;
  int t3 = tempId++;
  vector<IR *> irs3;
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  switch (root->type) {
  case EQ:
    irs3.push_back(new IR(BEQ, {new IRItem(IR_OFFSET, 3), new IRItem(TEMP, t1),
                                new IRItem(TEMP, t2)}));
    break;
  case NE:
    irs3.push_back(new IR(BNE, {new IRItem(IR_OFFSET, 3), new IRItem(TEMP, t1),
                                new IRItem(TEMP, t2)}));
    break;
  default:
    break;
  }
  irs3.push_back(new IR(MOV, {new IRItem(TEMP, t3), new IRItem(INT, 0)}));
  irs3.push_back(new IR(GOTO, {new IRItem(IR_OFFSET, 2)}));
  irs3.push_back(new IR(MOV, {new IRItem(TEMP, t3), new IRItem(INT, 1)}));
  return irs3;
}

vector<IR *> IRParser::parseFloatLiteral(AST *root) {
  return {
      new IR(MOV, {new IRItem(TEMP, tempId++), new IRItem(FLOAT, root->fVal)})};
}

vector<IR *> IRParser::parseFuncCall(AST *root) {
  vector<IR *> irs;
  vector<IRItem *> items;
  if (root->symbol)
    items.push_back(new IRItem(SYMBOL, root->symbol));
  else
    items.push_back(new IRItem(PLT, root->name));
  for (AST *node : root->nodes) {
    vector<IR *> moreIRs = parseAST(node);
    irs.insert(irs.end(), moreIRs.begin(), moreIRs.end());
    items.push_back(new IRItem(TEMP, irs.back()->items.front()->tempId));
  }
  irs.push_back(new IR(CALL, items));
  irs.push_back(new IR(MOV, {new IRItem(TEMP, tempId++), new IRItem(RETURN)}));
  return irs;
}

vector<IR *> IRParser::parseFuncDef(AST *root) {
  vector<IR *> irs;
  for (AST *node : root->nodes) {
    vector<IR *> moreIRs = parseAST(node);
    irs.insert(irs.end(), moreIRs.begin(), moreIRs.end());
  }
  irs.push_back(new IR(FUNC_END));
  return irs;
}

vector<IR *> IRParser::parseIfStmt(AST *root) {
  vector<IR *> irs1 = parseAST(root->nodes[0]);
  vector<IR *> irs2 = parseAST(root->nodes[1]);
  vector<IR *> irs4;
  int t1 = irs1.back()->items.front()->tempId;
  irs4.insert(irs4.end(), irs1.begin(), irs1.end());
  if (root->nodes[2]) {
    irs4.push_back(new IR(BNE, {new IRItem(IR_OFFSET, irs2.size() + 2),
                                new IRItem(TEMP, t1), new IRItem(INT, 0)}));
    irs4.insert(irs4.end(), irs2.begin(), irs2.end());
    vector<IR *> irs3 = parseAST(root->nodes[2]);
    irs4.push_back(new IR(GOTO, {new IRItem(IR_OFFSET, irs3.size() + 1)}));
    irs4.insert(irs4.end(), irs3.begin(), irs3.end());
  } else {
    irs4.push_back(new IR(BNE, {new IRItem(IR_OFFSET, irs2.size() + 1),
                                new IRItem(TEMP, t1), new IRItem(INT, 0)}));
    irs4.insert(irs4.end(), irs2.begin(), irs2.end());
  }
  return irs4;
}

vector<IR *> IRParser::parseIntLiteral(AST *root) {
  return {
      new IR(MOV, {new IRItem(TEMP, tempId++), new IRItem(INT, root->iVal)})};
}

vector<IR *> IRParser::parseLAndExp(AST *root) {
  vector<IR *> irs1 = parseAST(root->nodes[0]);
  vector<IR *> irs2 = parseAST(root->nodes[1]);
  vector<IR *> irs3;
  int t1 = irs1.back()->items.front()->tempId;
  int t2 = irs2.back()->items.front()->tempId;
  int t3 = tempId++;
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.push_back(new IR(BEQ, {new IRItem(IR_OFFSET, irs2.size() + 2),
                              new IRItem(TEMP, t1), new IRItem(INT, 0)}));
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  irs3.push_back(new IR(BNE, {new IRItem(IR_OFFSET, 3), new IRItem(TEMP, t2),
                              new IRItem(INT, 0)}));
  irs3.push_back(new IR(MOV, {new IRItem(TEMP, t3), new IRItem(INT, 0)}));
  irs3.push_back(new IR(GOTO, {new IRItem(IR_OFFSET, 2)}));
  irs3.push_back(new IR(MOV, {new IRItem(TEMP, t3), new IRItem(INT, 1)}));
  return irs3;
}

vector<IR *> IRParser::parseLOrExp(AST *root) {
  vector<IR *> irs1 = parseAST(root->nodes[0]);
  vector<IR *> irs2 = parseAST(root->nodes[1]);
  vector<IR *> irs3;
  int t1 = irs1.back()->items.front()->tempId;
  int t2 = irs2.back()->items.front()->tempId;
  int t3 = tempId++;
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.push_back(new IR(BNE, {new IRItem(IR_OFFSET, irs2.size() + 4),
                              new IRItem(TEMP, t1), new IRItem(INT, 0)}));
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  irs3.push_back(new IR(BNE, {new IRItem(IR_OFFSET, 3), new IRItem(TEMP, t2),
                              new IRItem(INT, 0)}));
  irs3.push_back(new IR(MOV, {new IRItem(TEMP, t3), new IRItem(INT, 0)}));
  irs3.push_back(new IR(GOTO, {new IRItem(IR_OFFSET, 2)}));
  irs3.push_back(new IR(MOV, {new IRItem(TEMP, t3), new IRItem(INT, 1)}));
  return irs3;
}

vector<IR *> IRParser::parseLVal(AST *root) {
  if (root->nodes.empty())
    return {new IR(
        MOV, {new IRItem(TEMP, tempId++), new IRItem(SYMBOL, root->symbol)})};
  vector<IR *> irs;
  vector<pair<int, int>> temps;
  vector<int> sizes = {1};
  for (int i = root->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * root->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  int offset = 0;
  for (unsigned i = 0; i < root->nodes.size(); i++) {
    if (root->nodes[i]->astType == INT_LITERAL) {
      offset += sizes[i] * root->nodes[i]->iVal;
      continue;
    }
    vector<IR *> moreIRs = parseAST(root->nodes[i]);
    irs.insert(irs.end(), moreIRs.begin(), moreIRs.end());
    temps.emplace_back(moreIRs.back()->items.front()->tempId, sizes[i]);
  }
  int baseId = tempId++;
  irs.push_back(new IR(
      LOAD, {new IRItem(TEMP, baseId), new IRItem(SYMBOL, root->symbol)}));
  if (offset) {
    int t1 = tempId++;
    irs.push_back(new IR(MOV, {new IRItem(TEMP, t1), new IRItem(INT, offset)}));
    int t2 = tempId++;
    irs.push_back(new IR(ADD, {new IRItem(TEMP, t2), new IRItem(TEMP, baseId),
                               new IRItem(TEMP, t1)}));
    baseId = t2;
  }
  for (pair<int, int> temp : temps) {
    int t1 = temp.first;
    int t2 = tempId++;
    irs.push_back(
        new IR(MOV, {new IRItem(TEMP, t2), new IRItem(INT, temp.second)}));
    int t3 = tempId++;
    irs.push_back(new IR(MUL, {new IRItem(TEMP, t3), new IRItem(TEMP, t1),
                               new IRItem(TEMP, t2)}));
    int t4 = tempId++;
    irs.push_back(new IR(ADD, {new IRItem(TEMP, t4), new IRItem(TEMP, baseId),
                               new IRItem(TEMP, t3)}));
    baseId = t4;
  }
  return irs;
}

vector<IR *> IRParser::parseMulExp(AST *root) {
  vector<IR *> irs1 = parseAST(root->nodes[0]);
  vector<IR *> irs2 = parseAST(root->nodes[1]);
  int t1 = irs1.back()->items.front()->tempId;
  int t2 = irs2.back()->items.front()->tempId;
  int t3 = tempId++;
  vector<IR *> irs3;
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  switch (root->type) {
  case DIV:
    irs3.push_back(new IR(DIV, {new IRItem(TEMP, t3), new IRItem(TEMP, t1),
                                new IRItem(TEMP, t2)}));
    break;
  case MOD:
    irs3.push_back(new IR(MOD, {new IRItem(TEMP, t3), new IRItem(TEMP, t1),
                                new IRItem(TEMP, t2)}));
    break;
  case MUL:
    irs3.push_back(new IR(MUL, {new IRItem(TEMP, t3), new IRItem(TEMP, t1),
                                new IRItem(TEMP, t2)}));
    break;
  default:
    break;
  }
  return irs3;
}

vector<IR *> IRParser::parseRelExp(AST *root) {
  vector<IR *> irs1 = parseAST(root->nodes[0]);
  vector<IR *> irs2 = parseAST(root->nodes[1]);
  int t1 = irs1.back()->items.front()->tempId;
  int t2 = irs2.back()->items.front()->tempId;
  int t3 = tempId++;
  vector<IR *> irs3;
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  switch (root->type) {
  case GE:
    irs3.push_back(new IR(BGE, {new IRItem(IR_OFFSET, 4), new IRItem(TEMP, t1),
                                new IRItem(TEMP, t2)}));
    break;
  case GT:
    irs3.push_back(new IR(BGT, {new IRItem(IR_OFFSET, 4), new IRItem(TEMP, t1),
                                new IRItem(TEMP, t2)}));
    break;
  case LE:
    irs3.push_back(new IR(BLE, {new IRItem(IR_OFFSET, 4), new IRItem(TEMP, t1),
                                new IRItem(TEMP, t2)}));
    break;
  case LT:
    irs3.push_back(new IR(BLT, {new IRItem(IR_OFFSET, 4), new IRItem(TEMP, t1),
                                new IRItem(TEMP, t2)}));
    break;
  default:
    break;
  }
  irs3.push_back(new IR(MOV, {new IRItem(TEMP, t3), new IRItem(INT, 0)}));
  irs3.push_back(new IR(GOTO, {new IRItem(IR_OFFSET, 2)}));
  irs3.push_back(new IR(MOV, {new IRItem(TEMP, t3), new IRItem(INT, 1)}));
  return irs3;
}

vector<IR *> IRParser::parseReturnStmt(AST *root) {
  if (root->nodes.empty())
    return {new IR(RETURN)};
  vector<IR *> irs = parseAST(root->nodes.front());
  irs.push_back(
      new IR(RETURN, {new IRItem(TEMP, irs.back()->items.front()->tempId)}));
  return irs;
}

void IRParser::parseRoot(AST *root) {
  this->isProcessed = true;
  for (AST *node : root->nodes) {
    switch (node->astType) {
    case CONST:
      consts.push_back(node->symbol);
      break;
    case FUNC_DEF:
      funcs[node->symbol] = parseFuncDef(node);
      break;
    case GLOBAL_VAR_DEF:
      globalVars.push_back(node->symbol);
      break;
    default:
      break;
    }
  }
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcs.begin();
       it != funcs.end(); it++) {
    vector<IR *> irs = it->second;
    for (unsigned j = 0; j < irs.size(); j++) {
      if (!irs[j]->items.empty() && irs[j]->items.front()->type == IR_OFFSET)
        irs[j]->items.front() =
            new IRItem(IR_T, irs[j + irs[j]->items.front()->offset]);
      switch (irs[j]->type) {
      case BREAK:
        for (unsigned k = j + 1; k < irs.size(); k++) {
          if (irs[k]->type == LABEL_WHILE_END) {
            delete irs[j];
            irs[j] = new IR(GOTO, {new IRItem(IR_T, irs[k + 1])});
            break;
          }
        }
        break;
      case CONTINUE:
        for (int k = j - 1; k >= 0; k--) {
          if (irs[k]->type == LABEL_WHILE_BEGIN) {
            delete irs[j];
            irs[j] = new IR(GOTO, {new IRItem(IR_T, irs[k + 1])});
            break;
          }
        }
        break;
      default:
        break;
      }
    }
    vector<IR *> newIRs;
    for (IR *ir : irs)
      if (ir->type != LABEL_WHILE_BEGIN && ir->type != LABEL_WHILE_END)
        newIRs.push_back(ir);
    it->second = newIRs;
  }
}

vector<IR *> IRParser::parseUnaryExp(AST *root) {
  vector<IR *> irs = parseAST(root->nodes.front());
  switch (root->type) {
  case L_NOT: {
    int t1 = tempId++;
    irs.push_back(
        new IR(BNE, {new IRItem(IR_OFFSET, 4),
                     new IRItem(TEMP, irs.back()->items.front()->tempId),
                     new IRItem(INT, 0)}));
    irs.push_back(new IR(MOV, {new IRItem(TEMP, t1), new IRItem(INT, 1)}));
    irs.push_back(new IR(GOTO, {new IRItem(IR_OFFSET, 2)}));
    irs.push_back(new IR(MOV, {new IRItem(TEMP, t1), new IRItem(INT, 0)}));
    break;
  }
  case MINUS: {
    int t1 = tempId++;
    irs.push_back(
        new IR(NEG, {new IRItem(TEMP, t1),
                     new IRItem(TEMP, irs.back()->items.front()->tempId)}));
    break;
  }
  case PLUS:
    break;
  default:
    break;
  }
  return irs;
}

vector<IR *> IRParser::parseWhileStmt(AST *root) {
  vector<IR *> irs1 = parseAST(root->nodes[0]);
  vector<IR *> irs2 = parseAST(root->nodes[1]);
  vector<IR *> irs3;
  int t1 = irs1.back()->items.front()->tempId;
  irs3.push_back(new IR(LABEL_WHILE_BEGIN));
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.push_back(new IR(BNE, {new IRItem(IR_OFFSET, 2), new IRItem(TEMP, t1),
                              new IRItem(INT, 0)}));
  irs3.push_back(new IR(GOTO, {new IRItem(IR_OFFSET, irs2.size() + 3)}));
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  irs3.push_back(
      new IR(GOTO, {new IRItem(IR_OFFSET, -(irs1.size() + irs2.size() + 2))}));
  irs3.push_back(new IR(LABEL_WHILE_END));
  return irs3;
}

void IRParser::printIRs() {
  if (!isProcessed)
    parseRoot(root);
  for (Symbol *cst : consts)
    cout << cst->toString() << endl;
  for (Symbol *cst : globalVars)
    cout << cst->toString() << endl;
  for (pair<Symbol *, vector<IR *>> func : funcs) {
    cout << func.first->name << endl;
    for (IR *ir : func.second)
      cout << ir->toString() << endl;
  }
}