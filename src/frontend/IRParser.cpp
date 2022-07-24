#include <algorithm>
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

IRItem *IRParser::lastResult(const vector<IR *> &irs) {
  for (int i = irs.size() - 1; i >= 0; i--)
    if (irs[i]->type != IR::LABEL && irs[i]->type != IR::LABEL_WHILE_BEGIN &&
        irs[i]->type != IR::LABEL_WHILE_END)
      return irs[i]->items[0];
  return nullptr;
}

vector<IR *> IRParser::parseAST(AST *root, Symbol *func) {
  switch (root->astType) {
  case AST::ASSIGN_STMT:
    return parseAssignStmt(root, func);
  case AST::BINARY_EXP:
    return parseBinaryExp(root, func);
  case AST::BLANK_STMT:
    return {};
  case AST::BLOCK:
    return parseBlock(root, func);
  case AST::BREAK_STMT:
    return {new IR(IR::BREAK)};
  case AST::CONTINUE_STMT:
    return {new IR(IR::CONTINUE)};
  case AST::EXP_STMT:
    return parseAST(root->nodes[0], func);
  case AST::FLOAT:
    return {new IR(IR::MOV, {new IRItem(IRItem::FTEMP, tempId++),
                             new IRItem(root->fVal)})};
  case AST::FUNC_CALL:
    return parseFuncCall(root, func);
  case AST::IF_STMT:
    return parseIfStmt(root, func);
  case AST::INT:
    return {new IR(IR::MOV, {new IRItem(IRItem::ITEMP, tempId++),
                             new IRItem(root->iVal)})};
  case AST::LOCAL_VAR_DEF:
    localVars[func].push_back(root->symbol);
    return {};
  case AST::L_VAL:
    return parseRVal(root, func);
  case AST::MEMSET_ZERO:
    return {new IR(IR::MEMSET_ZERO, {new IRItem(root->symbol)})};
  case AST::RETURN_STMT:
    return parseReturnStmt(root, func);
  case AST::UNARY_EXP:
    return parseUnaryExp(root, func);
  case AST::WHILE_STMT:
    return parseWhileStmt(root, func);
  default:
    break;
  }
  return {};
}

vector<IR *> IRParser::parseAlgoExp(AST *root, Symbol *func) {
  vector<IR *> irs1 = parseAST(root->nodes[0], func);
  vector<IR *> irs2 = parseAST(root->nodes[1], func);
  vector<IR *> irs3;
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  IR::IRType type = IR::ADD;
  switch (root->opType) {
  case AST::ADD:
    type = IR::ADD;
    break;
  case AST::DIV:
    type = IR::DIV;
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
  irs3.push_back(
      new IR(type, {new IRItem(lastResult(irs1)->type, tempId++),
                    lastResult(irs1)->clone(), lastResult(irs2)->clone()}));
  return irs3;
}

vector<IR *> IRParser::parseAssignStmt(AST *root, Symbol *func) {
  vector<IR *> irs1 = parseLVal(root->nodes[0], func);
  vector<IR *> irs2 = parseAST(root->nodes[1], func);
  vector<IR *> irs3;
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.back()->items.insert(irs3.back()->items.begin() + 1,
                            lastResult(irs2)->clone());
  return irs3;
}

vector<IR *> IRParser::parseBinaryExp(AST *root, Symbol *func) {
  switch (root->opType) {
  case AST::ADD:
  case AST::DIV:
  case AST::MUL:
  case AST::SUB:
    return parseAlgoExp(root, func);
  case AST::EQ:
  case AST::GE:
  case AST::GT:
  case AST::LE:
  case AST::LT:
  case AST::NE:
    return parseCmpExp(root, func);
  case AST::L_AND:
    return parseLAndExp(root, func);
  case AST::L_OR:
    return parseLOrExp(root, func);
  case AST::MOD:
    return parseModExp(root, func);
  default:
    break;
  }
  return {};
}

vector<IR *> IRParser::parseBlock(AST *root, Symbol *func) {
  vector<IR *> irs;
  for (AST *node : root->nodes) {
    vector<IR *> moreIRs = parseAST(node, func);
    irs.insert(irs.end(), moreIRs.begin(), moreIRs.end());
  }
  return irs;
}

vector<IR *> IRParser::parseCmpExp(AST *root, Symbol *func) {
  vector<IR *> irs1 = parseAST(root->nodes[0], func);
  vector<IR *> irs2 = parseAST(root->nodes[1], func);
  vector<IR *> irs3;
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  IR::IRType type = IR::BGE;
  switch (root->opType) {
  case AST::EQ:
    type = IR::EQ;
    break;
  case AST::GE:
    type = IR::GE;
    break;
  case AST::GT:
    type = IR::GT;
    break;
  case AST::LE:
    type = IR::LE;
    break;
  case AST::LT:
    type = IR::LT;
    break;
  case AST::NE:
    type = IR::NE;
    break;
  default:
    break;
  }
  irs3.push_back(
      new IR(type, {new IRItem(IRItem::ITEMP, tempId++),
                    lastResult(irs1)->clone(), lastResult(irs2)->clone()}));
  return irs3;
}

vector<IR *> IRParser::parseFuncCall(AST *root, Symbol *func) {
  vector<IR *> irs;
  IR *callIR = new IR(IR::CALL, {new IRItem(root->symbol)});
  for (unsigned i = 0; i < root->nodes.size(); i++) {
    vector<IR *> moreIRs = parseAST(root->nodes[i], func);
    irs.insert(irs.end(), moreIRs.begin(), moreIRs.end());
    callIR->items.push_back(lastResult(moreIRs)->clone());
  }
  irs.push_back(callIR);
  if (root->symbol->dataType != Symbol::VOID)
    irs.push_back(new IR(
        IR::MOV,
        {new IRItem(root->symbol->dataType == Symbol::INT ? IRItem::ITEMP
                                                          : IRItem::FTEMP,
                    tempId++),
         new IRItem(IRItem::RETURN)}));
  return irs;
}

vector<IR *> IRParser::parseFuncDef(AST *root, Symbol *func) {
  vector<IR *> irs;
  for (AST *node : root->nodes) {
    vector<IR *> moreIRs = parseAST(node, func);
    irs.insert(irs.end(), moreIRs.begin(), moreIRs.end());
  }
  irs.push_back(new IR(IR::LABEL));
  return irs;
}

vector<IR *> IRParser::parseIfStmt(AST *root, Symbol *func) {
  vector<IR *> irs1 = parseAST(root->nodes[0], func);
  vector<IR *> irs2 = parseAST(root->nodes[1], func);
  vector<IR *> irs3;
  if (root->nodes[2])
    irs3 = parseAST(root->nodes[2], func);
  vector<IR *> irs4;
  irs4.insert(irs4.end(), irs1.begin(), irs1.end());
  IR *label1 = new IR(IR::LABEL);
  IR *label2 = new IR(IR::LABEL);
  if (root->nodes[2]) {
    irs4.push_back(new IR(
        IR::BEQ, {new IRItem(label1), lastResult(irs1)->clone(),
                  lastResult(irs1)->type == IRItem::ITEMP ? new IRItem(0)
                                                          : new IRItem(0.0f)}));
    irs4.insert(irs4.end(), irs2.begin(), irs2.end());
    irs4.push_back(new IR(IR::GOTO, {new IRItem(label2)}));
    irs4.push_back(label1);
    irs4.insert(irs4.end(), irs3.begin(), irs3.end());
    irs4.push_back(label2);
  } else {
    irs4.push_back(new IR(
        IR::BEQ, {new IRItem(label1), lastResult(irs1)->clone(),
                  lastResult(irs1)->type == IRItem::ITEMP ? new IRItem(0)
                                                          : new IRItem(0.0f)}));
    irs4.insert(irs4.end(), irs2.begin(), irs2.end());
    irs4.push_back(label1);
  }
  return irs4;
}

vector<IR *> IRParser::parseLAndExp(AST *root, Symbol *func) {
  vector<IR *> irs1 = parseAST(root->nodes[0], func);
  vector<IR *> irs2 = parseAST(root->nodes[1], func);
  vector<IR *> irs3;
  IR *label1 = new IR(IR::LABEL);
  IR *label2 = new IR(IR::LABEL);
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.push_back(new IR(IR::BEQ, {new IRItem(label1), lastResult(irs1)->clone(),
                                  lastResult(irs1)->type == IRItem::ITEMP
                                      ? new IRItem(0)
                                      : new IRItem(0.0f)}));
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  irs3.push_back(new IR(IR::BEQ, {new IRItem(label1), lastResult(irs2)->clone(),
                                  lastResult(irs2)->type == IRItem::ITEMP
                                      ? new IRItem(0)
                                      : new IRItem(0.0f)}));
  irs3.push_back(
      new IR(IR::MOV, {new IRItem(IRItem::ITEMP, tempId), new IRItem(1)}));
  irs3.push_back(new IR(IR::GOTO, {new IRItem(label2)}));
  irs3.push_back(label1);
  irs3.push_back(
      new IR(IR::MOV, {new IRItem(IRItem::ITEMP, tempId++), new IRItem(0)}));
  irs3.push_back(label2);
  return irs3;
}

vector<IR *> IRParser::parseLOrExp(AST *root, Symbol *func) {
  vector<IR *> irs1 = parseAST(root->nodes[0], func);
  vector<IR *> irs2 = parseAST(root->nodes[1], func);
  vector<IR *> irs3;
  IR *label1 = new IR(IR::LABEL);
  IR *label2 = new IR(IR::LABEL);
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.push_back(new IR(IR::BNE, {new IRItem(label1), lastResult(irs1)->clone(),
                                  lastResult(irs1)->type == IRItem::ITEMP
                                      ? new IRItem(0)
                                      : new IRItem(0.0f)}));
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  irs3.push_back(new IR(IR::BNE, {new IRItem(label1), lastResult(irs2)->clone(),
                                  lastResult(irs2)->type == IRItem::ITEMP
                                      ? new IRItem(0)
                                      : new IRItem(0.0f)}));
  irs3.push_back(
      new IR(IR::MOV, {new IRItem(IRItem::ITEMP, tempId), new IRItem(0)}));
  irs3.push_back(new IR(IR::GOTO, {new IRItem(label2)}));
  irs3.push_back(label1);
  irs3.push_back(
      new IR(IR::MOV, {new IRItem(IRItem::ITEMP, tempId++), new IRItem(1)}));
  irs3.push_back(label2);
  return irs3;
}

vector<IR *> IRParser::parseLVal(AST *root, Symbol *func) {
  vector<IR *> irs;
  IR *lastIr = new IR(IR::MOV, {new IRItem(root->symbol)});
  for (unsigned i = 0; i < root->nodes.size(); i++) {
    vector<IR *> moreIRs = parseAST(root->nodes[i], func);
    irs.insert(irs.end(), moreIRs.begin(), moreIRs.end());
    lastIr->items.push_back(lastResult(moreIRs)->clone());
  }
  irs.push_back(lastIr);
  return irs;
}

vector<IR *> IRParser::parseModExp(AST *root, Symbol *func) {
  vector<IR *> irs1 = parseAST(root->nodes[0], func);
  vector<IR *> irs2 = parseAST(root->nodes[1], func);
  vector<IR *> irs3;
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  irs3.push_back(
      new IR(IR::DIV, {new IRItem(IRItem::ITEMP, tempId++),
                       lastResult(irs1)->clone(), lastResult(irs2)->clone()}));
  irs3.push_back(
      new IR(IR::MUL, {new IRItem(IRItem::ITEMP, tempId++),
                       lastResult(irs2)->clone(), lastResult(irs3)->clone()}));
  irs3.push_back(
      new IR(IR::SUB, {new IRItem(IRItem::ITEMP, tempId++),
                       lastResult(irs1)->clone(), lastResult(irs3)->clone()}));
  return irs3;
}

vector<IR *> IRParser::parseRVal(AST *root, Symbol *func) {
  vector<IR *> irs;
  IR *lastIr = new IR(
      IR::MOV,
      {new IRItem(root->symbol->dimensions.size() == root->nodes.size() &&
                          root->symbol->dataType == Symbol::FLOAT
                      ? IRItem::FTEMP
                      : IRItem::ITEMP,
                  tempId++),
       new IRItem(root->symbol)});
  for (unsigned i = 0; i < root->nodes.size(); i++) {
    vector<IR *> moreIRs = parseAST(root->nodes[i], func);
    irs.insert(irs.end(), moreIRs.begin(), moreIRs.end());
    lastIr->items.push_back(lastResult(moreIRs)->clone());
  }
  irs.push_back(lastIr);
  return irs;
}

vector<IR *> IRParser::parseReturnStmt(AST *root, Symbol *func) {
  if (root->nodes.empty())
    return {new IR(IR::RETURN)};
  vector<IR *> irs = parseAST(root->nodes[0], func);
  irs.push_back(new IR(IR::RETURN, {lastResult(irs)->clone()}));
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
      funcs.emplace_back(node->symbol, parseFuncDef(node, node->symbol));
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
      int count = 1;
      switch (irs[j]->type) {
      case IR::BREAK:
        for (unsigned k = j + 1; k < irs.size(); k++) {
          if (irs[k]->type == IR::LABEL_WHILE_BEGIN)
            count++;
          else if (irs[k]->type == IR::LABEL_WHILE_END) {
            count--;
            if (!count) {
              while (irs[k + 1]->type == IR::LABEL_WHILE_BEGIN ||
                     irs[k + 1]->type == IR::LABEL_WHILE_END)
                k++;
              irs[j]->type = IR::GOTO;
              irs[j]->items.push_back(new IRItem(irs[k]));
              break;
            }
          }
        }
        break;
      case IR::CONTINUE:
        for (int k = j - 1; k >= 0; k--) {
          if (irs[k]->type == IR::LABEL_WHILE_END)
            count++;
          else if (irs[k]->type == IR::LABEL_WHILE_BEGIN) {
            count--;
            if (!count) {
              while (irs[k + 1]->type == IR::LABEL_WHILE_BEGIN ||
                     irs[k + 1]->type == IR::LABEL_WHILE_END)
                k++;
              irs[j]->type = IR::GOTO;
              irs[j]->items.push_back(new IRItem(irs[k]));
              break;
            }
          }
        }
        break;
      default:
        break;
      }
    }
    vector<IR *> newIRs;
    for (IR *ir : irs)
      if (ir->type == IR::LABEL_WHILE_BEGIN || ir->type == IR::LABEL_WHILE_END)
        ir->type = IR::LABEL;
  }
}

vector<IR *> IRParser::parseUnaryExp(AST *root, Symbol *func) {
  vector<IR *> irs = parseAST(root->nodes[0], func);
  switch (root->opType) {
  case AST::F2I:
    irs.push_back(new IR(IR::F2I, {new IRItem(IRItem::ITEMP, tempId++),
                                   lastResult(irs)->clone()}));
    break;
  case AST::I2F:
    irs.push_back(new IR(IR::I2F, {new IRItem(IRItem::FTEMP, tempId++),
                                   lastResult(irs)->clone()}));
    break;
  case AST::L_NOT:
    irs.push_back(new IR(IR::L_NOT, {new IRItem(IRItem::ITEMP, tempId++),
                                     lastResult(irs)->clone()}));
    break;
  case AST::NEG:
    irs.push_back(new IR(IR::NEG, {new IRItem(lastResult(irs)->type, tempId++),
                                   lastResult(irs)->clone()}));
    break;
  default:
    break;
  }
  return irs;
}

vector<IR *> IRParser::parseWhileStmt(AST *root, Symbol *func) {
  vector<IR *> irs1 = parseAST(root->nodes[0], func);
  vector<IR *> irs2 = parseAST(root->nodes[1], func);
  vector<IR *> irs3;
  IR *label1 = new IR(IR::LABEL_WHILE_BEGIN);
  IR *label2 = new IR(IR::LABEL_WHILE_END);
  irs3.push_back(label1);
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.push_back(new IR(IR::BEQ, {new IRItem(label2), lastResult(irs1)->clone(),
                                  lastResult(irs1)->type == IRItem::ITEMP
                                      ? new IRItem(0)
                                      : new IRItem(0.0f)}));
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  irs3.push_back(new IR(IR::GOTO, {new IRItem(label1)}));
  irs3.push_back(label2);
  return irs3;
}

vector<Symbol *> IRParser::getConsts() {
  if (!isProcessed)
    parseRoot(root);
  return consts;
}

vector<Symbol *> IRParser::getGlobalVars() {
  if (!isProcessed)
    parseRoot(root);
  return globalVars;
}

unordered_map<Symbol *, vector<Symbol *>> IRParser::getLocalVars() {
  if (!isProcessed)
    parseRoot(root);
  return localVars;
}

vector<pair<Symbol *, vector<IR *>>> IRParser::getFuncs() {
  if (!isProcessed)
    parseRoot(root);
  return funcs;
}

unsigned IRParser::getTempId() { return tempId; }