#include <algorithm>
#include <iostream>
#include <queue>
#include <utility>

#include "IRParser.h"

using namespace std;

IRParser::IRParser(AST *root, vector<Symbol *> &symbols) {
  this->isProcessed = false;
  this->tempId = 0;
  this->root = root;
  this->symbols = symbols;
}

IRParser::~IRParser() {
  for (const pair<Symbol *, vector<IR *>> &func : funcs)
    for (IR *ir : func.second)
      delete ir;
}

vector<IR *> IRParser::parseAlgoExp(AST *root) {
  vector<IR *> irs1 = parseAST(root->nodes[0]);
  vector<IR *> irs2 = parseAST(root->nodes[1]);
  int t1 = irs1.back()->items[0]->iVal;
  int t2 = irs2.back()->items[0]->iVal;
  irs1.reserve(irs1.size() + irs2.size() + 1);
  irs1.insert(irs1.end(), irs2.begin(), irs2.end());
  IR::IRType type = IR::ADD;
  switch (root->opType) {
  case AST::ADD:
    type = IR::ADD;
    break;
  case AST::DIV:
    type = IR::DIV;
    break;
  case AST::MOD:
    type = IR::MOD;
    break;
  case AST::MUL:
    type = IR::MUL;
    break;
  case AST::SUB:
    type = IR::SUB;
    break;
  default:
    break;
  }
  irs1.push_back(new IR(type, {new IRItem(IRItem::TEMP, tempId++),
                               new IRItem(IRItem::TEMP, t1),
                               new IRItem(IRItem::TEMP, t2)}));
  return irs1;
}

vector<IR *> IRParser::parseAssignStmt(AST *root) {
  vector<IR *> irs1 = parseLVal(root->nodes[0]);
  vector<IR *> irs2 = parseAST(root->nodes[1]);
  int t1 = irs1.back()->items[0]->iVal;
  int t2 = irs2.back()->items[0]->iVal;
  irs2.reserve(irs1.size() + irs2.size() + 1);
  irs2.insert(irs2.end(), irs1.begin(), irs1.end());
  irs2.push_back(new IR(
      IR::STORE, {new IRItem(IRItem::TEMP, t1), new IRItem(IRItem::TEMP, t2)}));
  return irs2;
}

vector<IR *> IRParser::parseAST(AST *root) {
  switch (root->astType) {
  case AST::ASSIGN_STMT:
    return parseAssignStmt(root);
  case AST::BINARY_EXP:
    return parseBinaryExp(root);
  case AST::BLANK_STMT:
    return {};
  case AST::BLOCK:
    return parseBlock(root);
  case AST::BREAK_STMT:
    return {new IR(IR::BREAK)};
  case AST::CONTINUE_STMT:
    return {new IR(IR::CONTINUE)};
  case AST::EXP_STMT:
    return parseAST(root->nodes[0]);
  case AST::FLOAT_LITERAL:
    return {new IR(IR::MOV, {new IRItem(IRItem::TEMP, tempId++),
                             new IRItem(IRItem::FLOAT, root->fVal)})};
  case AST::FUNC_CALL:
    return parseFuncCall(root);
  case AST::IF_STMT:
    return parseIfStmt(root);
  case AST::INT_LITERAL:
    return {new IR(IR::MOV, {new IRItem(IRItem::TEMP, tempId++),
                             new IRItem(IRItem::INT, root->iVal)})};
  case AST::L_VAL: {
    vector<IR *> irs = parseLVal(root);
    irs.push_back(new IR(
        IR::LOAD, {new IRItem(IRItem::TEMP, tempId++),
                   new IRItem(IRItem::TEMP, irs.back()->items[0]->iVal)}));
    return irs;
  }
  case AST::MEMSET_ZERO:
    return {
        new IR(IR::MEMSET_ZERO, {new IRItem(IRItem::SYMBOL, root->symbol)})};
  case AST::RETURN_STMT:
    return parseReturnStmt(root);
  case AST::UNARY_EXP:
    return parseUnaryExp(root);
  case AST::WHILE_STMT:
    return parseWhileStmt(root);
  default:
    break;
  }
  return {};
}

vector<IR *> IRParser::parseBinaryExp(AST *root) {
  switch (root->opType) {
  case AST::ADD:
  case AST::DIV:
  case AST::MOD:
  case AST::MUL:
  case AST::SUB:
    return parseAlgoExp(root);
  case AST::EQ:
  case AST::GE:
  case AST::GT:
  case AST::LE:
  case AST::LT:
  case AST::NE:
    return parseCmpExp(root);
  case AST::L_AND:
    return parseLAndExp(root);
  case AST::L_OR:
    return parseLOrExp(root);
  default:
    break;
  }
  return {};
}

vector<IR *> IRParser::parseBlock(AST *root) {
  vector<IR *> irs;
  for (AST *node : root->nodes) {
    vector<IR *> moreIRs = parseAST(node);
    irs.insert(irs.end(), moreIRs.begin(), moreIRs.end());
  }
  return irs;
}

vector<IR *> IRParser::parseCmpExp(AST *root) {
  vector<IR *> irs1 = parseAST(root->nodes[0]);
  vector<IR *> irs2 = parseAST(root->nodes[1]);
  int t1 = irs1.back()->items[0]->iVal;
  int t2 = irs2.back()->items[0]->iVal;
  irs1.reserve(irs1.size() + irs2.size() + 4);
  irs1.insert(irs1.end(), irs2.begin(), irs2.end());
  IR::IRType type = IR::BGE;
  switch (root->opType) {
  case AST::EQ:
    type = IR::BEQ;
    break;
  case AST::GE:
    type = IR::BGE;
    break;
  case AST::GT:
    type = IR::BGT;
    break;
  case AST::LE:
    type = IR::BLE;
    break;
  case AST::LT:
    type = IR::BLT;
    break;
  case AST::NE:
    type = IR::BNE;
    break;
  default:
    break;
  }
  irs1.push_back(new IR(type, {new IRItem(IRItem::IR_OFFSET, 3),
                               new IRItem(IRItem::TEMP, t1),
                               new IRItem(IRItem::TEMP, t2)}));
  irs1.push_back(new IR(
      IR::MOV, {new IRItem(IRItem::TEMP, tempId), new IRItem(IRItem::INT, 0)}));
  irs1.push_back(new IR(IR::GOTO, {new IRItem(IRItem::IR_OFFSET, 2)}));
  irs1.push_back(new IR(IR::MOV, {new IRItem(IRItem::TEMP, tempId++),
                                  new IRItem(IRItem::INT, 1)}));
  return irs1;
}

vector<IR *> IRParser::parseFuncCall(AST *root) {
  vector<IR *> irs;
  vector<IRItem *> items;
  items.push_back(root->symbol ? new IRItem(IRItem::SYMBOL, root->symbol)
                               : new IRItem(IRItem::PLT, root->name));
  for (AST *node : root->nodes) {
    vector<IR *> moreIRs = parseAST(node);
    irs.insert(irs.end(), moreIRs.begin(), moreIRs.end());
    items.push_back(new IRItem(IRItem::TEMP, irs.back()->items[0]->iVal));
  }
  irs.push_back(new IR(IR::CALL, items));
  irs.push_back(new IR(IR::MOV, {new IRItem(IRItem::TEMP, tempId++),
                                 new IRItem(IRItem::RETURN)}));
  return irs;
}

vector<IR *> IRParser::parseFuncDef(AST *root) {
  vector<IR *> irs;
  for (AST *node : root->nodes) {
    vector<IR *> moreIRs = parseAST(node);
    irs.insert(irs.end(), moreIRs.begin(), moreIRs.end());
  }
  irs.push_back(new IR(IR::FUNC_END));
  return irs;
}

vector<IR *> IRParser::parseIfStmt(AST *root) {
  vector<IR *> irs1 = parseAST(root->nodes[0]);
  vector<IR *> irs2 = parseAST(root->nodes[1]);
  int t1 = irs1.back()->items[0]->iVal;
  if (root->nodes[2]) {
    irs1.push_back(new IR(
        IR::BEQ, {new IRItem(IRItem::IR_OFFSET, (int)irs2.size() + 2),
                  new IRItem(IRItem::TEMP, t1), new IRItem(IRItem::INT, 0)}));
    irs1.insert(irs1.end(), irs2.begin(), irs2.end());
    vector<IR *> irs3 = parseAST(root->nodes[2]);
    irs1.push_back(new IR(
        IR::GOTO, {new IRItem(IRItem::IR_OFFSET, (int)irs3.size() + 1)}));
    irs1.insert(irs1.end(), irs3.begin(), irs3.end());
  } else {
    irs1.push_back(new IR(
        IR::BEQ, {new IRItem(IRItem::IR_OFFSET, (int)irs2.size() + 1),
                  new IRItem(IRItem::TEMP, t1), new IRItem(IRItem::INT, 0)}));
    irs1.insert(irs1.end(), irs2.begin(), irs2.end());
  }
  return irs1;
}

vector<IR *> IRParser::parseLAndExp(AST *root) {
  vector<IR *> irs1 = parseAST(root->nodes[0]);
  vector<IR *> irs2 = parseAST(root->nodes[1]);
  int t1 = irs1.back()->items[0]->iVal;
  int t2 = irs2.back()->items[0]->iVal;
  irs1.reserve(irs1.size() + irs2.size() + 5);
  irs1.push_back(new IR(
      IR::BEQ, {new IRItem(IRItem::IR_OFFSET, (int)irs2.size() + 2),
                new IRItem(IRItem::TEMP, t1), new IRItem(IRItem::INT, 0)}));
  irs1.insert(irs1.end(), irs2.begin(), irs2.end());
  irs1.push_back(new IR(IR::BNE, {new IRItem(IRItem::IR_OFFSET, 3),
                                  new IRItem(IRItem::TEMP, t2),
                                  new IRItem(IRItem::INT, 0)}));
  irs1.push_back(new IR(
      IR::MOV, {new IRItem(IRItem::TEMP, tempId), new IRItem(IRItem::INT, 0)}));
  irs1.push_back(new IR(IR::GOTO, {new IRItem(IRItem::IR_OFFSET, 2)}));
  irs1.push_back(new IR(IR::MOV, {new IRItem(IRItem::TEMP, tempId++),
                                  new IRItem(IRItem::INT, 1)}));
  return irs1;
}

vector<IR *> IRParser::parseLOrExp(AST *root) {
  vector<IR *> irs1 = parseAST(root->nodes[0]);
  vector<IR *> irs2 = parseAST(root->nodes[1]);
  int t1 = irs1.back()->items[0]->iVal;
  int t2 = irs2.back()->items[0]->iVal;
  irs1.reserve(irs1.size() + irs2.size() + 5);
  irs1.push_back(new IR(
      IR::BNE, {new IRItem(IRItem::IR_OFFSET, (int)irs2.size() + 4),
                new IRItem(IRItem::TEMP, t1), new IRItem(IRItem::INT, 0)}));
  irs1.insert(irs1.end(), irs2.begin(), irs2.end());
  irs1.push_back(new IR(IR::BNE, {new IRItem(IRItem::IR_OFFSET, 3),
                                  new IRItem(IRItem::TEMP, t2),
                                  new IRItem(IRItem::INT, 0)}));
  irs1.push_back(new IR(
      IR::MOV, {new IRItem(IRItem::TEMP, tempId), new IRItem(IRItem::INT, 0)}));
  irs1.push_back(new IR(IR::GOTO, {new IRItem(IRItem::IR_OFFSET, 2)}));
  irs1.push_back(new IR(IR::MOV, {new IRItem(IRItem::TEMP, tempId++),
                                  new IRItem(IRItem::INT, 1)}));
  return irs1;
}

vector<IR *> IRParser::parseLVal(AST *root) {
  if (root->nodes.empty())
    return {new IR(IR::MOV, {new IRItem(IRItem::TEMP, tempId++),
                             new IRItem(IRItem::SYMBOL, root->symbol)})};
  vector<IR *> irs;
  vector<pair<int, int>> temps;
  vector<int> sizes = {1};
  for (int i = root->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * root->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  int offset = 0;
  for (unsigned i = 0; i < root->nodes.size(); i++) {
    if (root->nodes[i]->astType == AST::INT_LITERAL) {
      offset += sizes[i] * root->nodes[i]->iVal;
      continue;
    }
    vector<IR *> moreIRs = parseAST(root->nodes[i]);
    irs.insert(irs.end(), moreIRs.begin(), moreIRs.end());
    temps.emplace_back(moreIRs.back()->items[0]->iVal, sizes[i]);
  }
  int baseId = tempId++;
  irs.push_back(new IR(IR::LOAD, {new IRItem(IRItem::TEMP, baseId),
                                  new IRItem(IRItem::SYMBOL, root->symbol)}));
  if (offset) {
    irs.push_back(new IR(IR::MOV, {new IRItem(IRItem::TEMP, tempId++),
                                   new IRItem(IRItem::INT, offset)}));
    irs.push_back(new IR(IR::ADD, {new IRItem(IRItem::TEMP, tempId),
                                   new IRItem(IRItem::TEMP, baseId),
                                   new IRItem(IRItem::TEMP, tempId - 1)}));
    baseId = tempId++;
  }
  for (pair<int, int> temp : temps) {
    int t1 = temp.first;
    int t2 = tempId++;
    int t3 = tempId++;
    int t4 = tempId++;
    irs.push_back(new IR(IR::MOV, {new IRItem(IRItem::TEMP, t2),
                                   new IRItem(IRItem::INT, temp.second)}));
    irs.push_back(new IR(IR::MUL, {new IRItem(IRItem::TEMP, t3),
                                   new IRItem(IRItem::TEMP, t1),
                                   new IRItem(IRItem::TEMP, t2)}));
    irs.push_back(new IR(IR::ADD, {new IRItem(IRItem::TEMP, t4),
                                   new IRItem(IRItem::TEMP, baseId),
                                   new IRItem(IRItem::TEMP, t3)}));
    baseId = t4;
  }
  return irs;
}

vector<IR *> IRParser::parseReturnStmt(AST *root) {
  if (root->nodes.empty())
    return {new IR(IR::RETURN)};
  vector<IR *> irs = parseAST(root->nodes[0]);
  irs.push_back(new IR(IR::RETURN,
                       {new IRItem(IRItem::TEMP, irs.back()->items[0]->iVal)}));
  return irs;
}

void IRParser::parseRoot(AST *root) {
  this->isProcessed = true;
  for (AST *node : root->nodes) {
    switch (node->astType) {
    case AST::CONST_DEF:
      consts.push_back(node->symbol);
      break;
    case AST::FUNC_DEF:
      funcs.emplace_back(node->symbol, parseFuncDef(node));
      break;
    case AST::GLOBAL_VAR_DEF:
      globalVars.push_back(node->symbol);
      break;
    default:
      break;
    }
  }
  for (unsigned i = 0; i < funcs.size(); i++) {
    vector<IR *> &irs = funcs[i].second;
    for (unsigned j = 0; j < irs.size(); j++) {
      switch (irs[j]->type) {
      case IR::BREAK:
        for (unsigned k = j + 1; k < irs.size(); k++)
          if (irs[k]->type == IR::LABEL_WHILE_END) {
            int l = k + 1;
            while (irs[l]->type == IR::LABEL_WHILE_BEGIN ||
                   irs[l]->type == IR::LABEL_WHILE_END)
              l++;
            irs[j]->type = IR::GOTO;
            irs[j]->items.push_back(new IRItem(IRItem::IR_T, irs[l]));
            break;
          }
        break;
      case IR::CONTINUE:
        for (int k = j - 1; k >= 0; k--)
          if (irs[k]->type == IR::LABEL_WHILE_BEGIN) {
            int l = k + 1;
            while (irs[l]->type == IR::LABEL_WHILE_BEGIN ||
                   irs[l]->type == IR::LABEL_WHILE_END)
              l++;
            irs[j]->type = IR::GOTO;
            irs[j]->items.push_back(new IRItem(IRItem::IR_T, irs[l]));
            break;
          }
        break;
      default:
        if (!irs[j]->items.empty() &&
            irs[j]->items[0]->type == IRItem::IR_OFFSET) {
          int index = j + irs[j]->items[0]->iVal;
          while (irs[index]->type == IR::LABEL_WHILE_BEGIN ||
                 irs[index]->type == IR::LABEL_WHILE_END)
            index++;
          delete irs[j]->items[0];
          irs[j]->items[0] = new IRItem(IRItem::IR_T, irs[index]);
        }
        break;
      }
    }
    vector<IR *> newIRs;
    for (IR *ir : irs)
      if (ir->type == IR::LABEL_WHILE_BEGIN || ir->type == IR::LABEL_WHILE_END)
        delete ir;
      else
        newIRs.push_back(ir);
    funcs[i].second = newIRs;
  }
}

vector<IR *> IRParser::parseUnaryExp(AST *root) {
  vector<IR *> irs = parseAST(root->nodes[0]);
  switch (root->opType) {
  case AST::L_NOT:
    irs.reserve(irs.size() + 4);
    irs.push_back(
        new IR(IR::BNE, {new IRItem(IRItem::IR_OFFSET, 4),
                         new IRItem(IRItem::TEMP, irs.back()->items[0]->iVal),
                         new IRItem(IRItem::INT, 0)}));
    irs.push_back(new IR(IR::MOV, {new IRItem(IRItem::TEMP, tempId),
                                   new IRItem(IRItem::INT, 1)}));
    irs.push_back(new IR(IR::GOTO, {new IRItem(IRItem::IR_OFFSET, 2)}));
    irs.push_back(new IR(IR::MOV, {new IRItem(IRItem::TEMP, tempId++),
                                   new IRItem(IRItem::INT, 0)}));
    break;
  case AST::NEG:
    irs.push_back(new IR(
        IR::NEG, {new IRItem(IRItem::TEMP, tempId++),
                  new IRItem(IRItem::TEMP, irs.back()->items[0]->iVal)}));
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
  irs3.reserve(irs1.size() + irs2.size() + 5);
  irs3.push_back(new IR(IR::LABEL_WHILE_BEGIN));
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.push_back(
      new IR(IR::BNE, {new IRItem(IRItem::IR_OFFSET, 2),
                       new IRItem(IRItem::TEMP, irs1.back()->items[0]->iVal),
                       new IRItem(IRItem::INT, 0)}));
  irs3.push_back(
      new IR(IR::GOTO, {new IRItem(IRItem::IR_OFFSET, (int)irs2.size() + 3)}));
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  irs3.push_back(
      new IR(IR::GOTO, {new IRItem(IRItem::IR_OFFSET,
                                   -(int)irs1.size() - (int)irs2.size() - 2)}));
  irs3.push_back(new IR(IR::LABEL_WHILE_END));
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