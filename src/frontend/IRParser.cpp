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
  switch (root->type) {
  case AST::ADD_EXP:
  case AST::DIV_EXP:
  case AST::MOD_EXP:
  case AST::MUL_EXP:
  case AST::SUB_EXP:
    return parseAlgoExp(root, func);
  case AST::ASSIGN_STMT:
    return parseAssignStmt(root, func);
  case AST::BLANK_STMT:
    return {};
  case AST::BLOCK:
    return parseBlock(root, func);
  case AST::BREAK_STMT:
    return {new IR(IR::BREAK)};
  case AST::CONTINUE_STMT:
    return {new IR(IR::CONTINUE)};
  case AST::EQ_EXP:
  case AST::GE_EXP:
  case AST::GT_EXP:
  case AST::LE_EXP:
  case AST::LT_EXP:
  case AST::NE_EXP:
    return parseCmpExp(root, func);
  case AST::EXP_STMT:
    return parseAST(root->nodes[0], func);
  case AST::F2I_EXP: {
    vector<IR *> irs = parseAST(root->nodes[0], func);
    irs.push_back(new IR(IR::F2I, {new IRItem(IRItem::ITEMP, tempId++),
                                   lastResult(irs)->clone()}));
    return irs;
  }
  case AST::FLOAT:
    return {new IR(IR::MOV, {new IRItem(IRItem::FTEMP, tempId++),
                             new IRItem(root->fVal)})};
  case AST::FUNC_CALL:
    return parseFuncCall(root, func);
  case AST::I2F_EXP: {
    vector<IR *> irs = parseAST(root->nodes[0], func);
    irs.push_back(new IR(IR::I2F, {new IRItem(IRItem::FTEMP, tempId++),
                                   lastResult(irs)->clone()}));
    return irs;
  }
  case AST::IF_STMT:
    return parseIfStmt(root, func);
  case AST::INT:
    return {new IR(IR::MOV, {new IRItem(IRItem::ITEMP, tempId++),
                             new IRItem(root->iVal)})};
  case AST::LOCAL_VAR_DEF:
    localVars[func].push_back(root->symbol);
    return {};
  case AST::L_NOT_EXP: {
    vector<IR *> irs = parseAST(root->nodes[0], func);
    irs.push_back(new IR(IR::L_NOT, {new IRItem(IRItem::ITEMP, tempId++),
                                     lastResult(irs)->clone()}));
    return irs;
  }
  case AST::L_VAL:
    return parseRVal(root, func);
  case AST::MEMSET_ZERO:
    return {new IR(IR::MEMSET_ZERO, {new IRItem(root->symbol)})};
  case AST::NEG_EXP: {
    vector<IR *> irs = parseAST(root->nodes[0], func);
    irs.push_back(new IR(IR::NEG, {new IRItem(lastResult(irs)->type, tempId++),
                                   lastResult(irs)->clone()}));
    return irs;
  }
  case AST::RETURN_STMT:
    return parseReturnStmt(root, func);
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
  switch (root->type) {
  case AST::ADD_EXP:
    type = IR::ADD;
    break;
  case AST::DIV_EXP:
    type = IR::DIV;
    break;
  case AST::MOD_EXP:
    type = IR::MOD;
    break;
  case AST::MUL_EXP:
    type = IR::MUL;
    break;
  case AST::SUB_EXP:
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
  switch (root->type) {
  case AST::EQ_EXP:
    type = IR::EQ;
    break;
  case AST::GE_EXP:
    type = IR::GE;
    break;
  case AST::GT_EXP:
    type = IR::GT;
    break;
  case AST::LE_EXP:
    type = IR::LE;
    break;
  case AST::LT_EXP:
    type = IR::LT;
    break;
  case AST::NE_EXP:
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

vector<IR *> IRParser::parseCond(AST *root, Symbol *func, IR *trueLabel,
                                 IR *falseLabel) {
  switch (root->type) {
  case AST::ADD_EXP:
  case AST::DIV_EXP:
  case AST::FUNC_CALL:
  case AST::L_VAL:
  case AST::MOD_EXP:
  case AST::MUL_EXP:
  case AST::SUB_EXP: {
    vector<IR *> irs = parseAST(root, func);
    irs.push_back(new IR(
        IR::BNE, {new IRItem(trueLabel), lastResult(irs)->clone(),
                  lastResult(irs)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                         : new IRItem(0)}));
    irs.push_back(new IR(IR::GOTO, {new IRItem(falseLabel)}));
    return irs;
  }
  case AST::EQ_EXP:
  case AST::GE_EXP:
  case AST::GT_EXP:
  case AST::LE_EXP:
  case AST::LT_EXP:
  case AST::NE_EXP:
    return parseLCmpExp(root, func, trueLabel, falseLabel);
  case AST::L_AND_EXP:
    return parseLAndExp(root, func, trueLabel, falseLabel);
  case AST::L_OR_EXP:
    return parseLOrExp(root, func, trueLabel, falseLabel);
  case AST::FLOAT:
    return {
        new IR(IR::GOTO, {new IRItem(root->fVal ? trueLabel : falseLabel)})};
  case AST::INT:
    return {
        new IR(IR::GOTO, {new IRItem(root->iVal ? trueLabel : falseLabel)})};
  case AST::F2I_EXP: {
    vector<IR *> irs = parseAST(root, func);
    irs.push_back(new IR(IR::BNE, {new IRItem(trueLabel),
                                   lastResult(irs)->clone(), new IRItem(0)}));
    irs.push_back(new IR(IR::GOTO, {new IRItem(falseLabel)}));
    return irs;
  }
  case AST::I2F_EXP: {
    vector<IR *> irs = parseAST(root->nodes[0], func);
    irs.push_back(new IR(IR::BNE, {new IRItem(trueLabel),
                                   lastResult(irs)->clone(), new IRItem(0)}));
    irs.push_back(new IR(IR::GOTO, {new IRItem(falseLabel)}));
    return irs;
  }
  case AST::L_NOT_EXP: {
    if (root->nodes[0]->type == AST::L_NOT_EXP)
      return parseCond(root->nodes[0]->nodes[0], func, trueLabel, falseLabel);
    vector<IR *> irs = parseAST(root->nodes[0], func);
    irs.push_back(new IR(IR::BEQ, {new IRItem(trueLabel),
                                   lastResult(irs)->clone(), new IRItem(0)}));
    irs.push_back(new IR(IR::GOTO, {new IRItem(falseLabel)}));
    return irs;
  }
  case AST::NEG_EXP:
    return parseCond(root->nodes[0], func, trueLabel, falseLabel);
  default:
    break;
  }
  return {};
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
  funcEnd = new IR(IR::LABEL);
  for (AST *node : root->nodes) {
    vector<IR *> moreIRs = parseAST(node, func);
    irs.insert(irs.end(), moreIRs.begin(), moreIRs.end());
  }
  irs.push_back(funcEnd);
  return irs;
}

vector<IR *> IRParser::parseIfStmt(AST *root, Symbol *func) {
  IR *label0 = new IR(IR::LABEL);
  IR *label1 = new IR(IR::LABEL);
  IR *label2 = new IR(IR::LABEL);
  vector<IR *> irs1 = parseCond(root->nodes[0], func, label0, label1);
  vector<IR *> irs2 = parseAST(root->nodes[1], func);
  vector<IR *> irs3;
  if (root->nodes[2])
    irs3 = parseAST(root->nodes[2], func);
  vector<IR *> irs4;
  irs4.insert(irs4.end(), irs1.begin(), irs1.end());
  irs4.push_back(label0);
  irs4.insert(irs4.end(), irs2.begin(), irs2.end());
  if (root->nodes[2]) {
    irs4.push_back(new IR(IR::GOTO, {new IRItem(label2)}));
    irs4.push_back(label1);
    irs4.insert(irs4.end(), irs3.begin(), irs3.end());
    irs4.push_back(label2);
  } else {
    irs4.push_back(label1);
    delete label2;
  }
  return irs4;
}

vector<IR *> IRParser::parseLAndExp(AST *root, Symbol *func, IR *trueLabel,
                                    IR *falseLabel) {
  IR *label = new IR(IR::LABEL);
  AST *left = root->nodes[0];
  AST *right = root->nodes[1];
  vector<IR *> irs1;
  switch (left->type) {
  case AST::ADD_EXP:
  case AST::DIV_EXP:
  case AST::FUNC_CALL:
  case AST::L_VAL:
  case AST::MOD_EXP:
  case AST::MUL_EXP:
  case AST::SUB_EXP:
    irs1 = parseAST(left, func);
    irs1.push_back(new IR(
        IR::BEQ, {new IRItem(falseLabel), lastResult(irs1)->clone(),
                  lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    break;
  case AST::EQ_EXP:
  case AST::GE_EXP:
  case AST::GT_EXP:
  case AST::LE_EXP:
  case AST::LT_EXP:
  case AST::NE_EXP:
    irs1 = parseLCmpExp(left, func, label, falseLabel);
    break;
  case AST::L_AND_EXP:
    irs1 = parseLAndExp(left, func, label, falseLabel);
    break;
  case AST::FLOAT:
    irs1 = {new IR(IR::GOTO, {new IRItem(left->fVal ? label : falseLabel)})};
    break;
  case AST::INT:
    irs1 = {new IR(IR::GOTO, {new IRItem(root->iVal ? label : falseLabel)})};
    break;
  case AST::F2I_EXP:
    irs1 = parseAST(left, func);
    irs1.push_back(new IR(IR::BEQ, {new IRItem(falseLabel),
                                    lastResult(irs1)->clone(), new IRItem(0)}));
    break;
  case AST::I2F_EXP:
    irs1 = parseAST(left->nodes[0], func);
    irs1.push_back(new IR(IR::BEQ, {new IRItem(falseLabel),
                                    lastResult(irs1)->clone(), new IRItem(0)}));
    break;
  case AST::L_NOT_EXP:
    if (left->nodes[0]->type == AST::L_NOT_EXP) {
      irs1 = parseAST(left->nodes[0]->nodes[0], func);
      irs1.push_back(new IR(
          IR::BEQ, {new IRItem(falseLabel), lastResult(irs1)->clone(),
                    lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                            : new IRItem(0)}));
      break;
    }
    irs1 = parseAST(left->nodes[0], func);
    irs1.push_back(new IR(
        IR::BNE, {new IRItem(falseLabel), lastResult(irs1)->clone(),
                  lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    break;
  case AST::NEG_EXP:
    irs1 = parseAST(left->nodes[0], func);
    irs1.push_back(new IR(
        IR::BEQ, {new IRItem(falseLabel), lastResult(irs1)->clone(),
                  lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    break;
  default:
    break;
  }
  vector<IR *> irs2;
  switch (right->type) {
  case AST::ADD_EXP:
  case AST::DIV_EXP:
  case AST::FUNC_CALL:
  case AST::L_VAL:
  case AST::MOD_EXP:
  case AST::MUL_EXP:
  case AST::SUB_EXP:
    irs2 = parseAST(right, func);
    irs2.push_back(new IR(
        IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                  lastResult(irs2)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
    break;
  case AST::EQ_EXP:
  case AST::GE_EXP:
  case AST::GT_EXP:
  case AST::LE_EXP:
  case AST::LT_EXP:
  case AST::NE_EXP:
    irs2 = parseLCmpExp(right, func, trueLabel, falseLabel);
    break;
  case AST::L_AND_EXP:
    irs2 = parseLAndExp(right, func, trueLabel, falseLabel);
    break;
  case AST::FLOAT:
    irs2 = {
        new IR(IR::GOTO, {new IRItem(right->fVal ? trueLabel : falseLabel)})};
    break;
  case AST::INT:
    irs2 = {
        new IR(IR::GOTO, {new IRItem(root->iVal ? trueLabel : falseLabel)})};
    break;
  case AST::F2I_EXP:
    irs2 = parseAST(right, func);
    irs2.push_back(new IR(IR::BEQ, {new IRItem(falseLabel),
                                    lastResult(irs2)->clone(), new IRItem(0)}));
    irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
    break;
  case AST::I2F_EXP:
    irs2 = parseAST(right->nodes[0], func);
    irs2.push_back(new IR(IR::BEQ, {new IRItem(falseLabel),
                                    lastResult(irs2)->clone(), new IRItem(0)}));
    irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
    break;
  case AST::L_NOT_EXP:
    if (right->nodes[0]->type == AST::L_NOT_EXP) {
      irs2 = parseAST(right->nodes[0]->nodes[0], func);
      irs2.push_back(new IR(
          IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                    lastResult(irs2)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                            : new IRItem(0)}));
      irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
      break;
    }
    irs2 = parseAST(right->nodes[0], func);
    irs2.push_back(new IR(
        IR::BNE, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                  lastResult(irs2)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
    break;
  case AST::NEG_EXP:
    irs2 = parseAST(right->nodes[0], func);
    irs2.push_back(new IR(
        IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                  lastResult(irs2)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
    break;
  default:
    break;
  }
  vector<IR *> irs3;
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.push_back(label);
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  return irs3;
}

vector<IR *> IRParser::parseLCmpExp(AST *root, Symbol *func, IR *trueLabel,
                                    IR *falseLabel) {
  AST *left = root->nodes[0];
  AST *right = root->nodes[1];
  vector<IR *> irs1;
  switch (left->type) {
  case AST::ADD_EXP:
  case AST::DIV_EXP:
  case AST::EQ_EXP:
  case AST::F2I_EXP:
  case AST::FUNC_CALL:
  case AST::GE_EXP:
  case AST::GT_EXP:
  case AST::I2F_EXP:
  case AST::L_NOT_EXP:
  case AST::L_VAL:
  case AST::LE_EXP:
  case AST::LT_EXP:
  case AST::MOD_EXP:
  case AST::MUL_EXP:
  case AST::NE_EXP:
  case AST::NEG_EXP:
  case AST::SUB_EXP:
    irs1 = parseAST(left, func);
    break;
  case AST::FLOAT:
    irs1 = {new IR(IR::MOV, {new IRItem(IRItem::FTEMP, tempId++),
                             new IRItem(left->fVal)})};
    break;
  case AST::INT:
    irs1 = {new IR(IR::MOV, {new IRItem(IRItem::ITEMP, tempId++),
                             new IRItem(left->iVal)})};
    break;
  default:
    break;
  }
  vector<IR *> irs2;
  switch (right->type) {
  case AST::ADD_EXP:
  case AST::DIV_EXP:
  case AST::EQ_EXP:
  case AST::F2I_EXP:
  case AST::FUNC_CALL:
  case AST::GE_EXP:
  case AST::GT_EXP:
  case AST::I2F_EXP:
  case AST::L_NOT_EXP:
  case AST::L_VAL:
  case AST::LE_EXP:
  case AST::LT_EXP:
  case AST::MOD_EXP:
  case AST::MUL_EXP:
  case AST::NE_EXP:
  case AST::NEG_EXP:
  case AST::SUB_EXP:
    irs2 = parseAST(right, func);
    break;
  case AST::FLOAT:
    irs2 = {new IR(IR::MOV, {new IRItem(IRItem::FTEMP, tempId++),
                             new IRItem(right->fVal)})};
    break;
  case AST::INT:
    irs2 = {new IR(IR::MOV, {new IRItem(IRItem::ITEMP, tempId++),
                             new IRItem(right->iVal)})};
    break;
  default:
    break;
  }
  IR::IRType type = IR::BEQ;
  switch (root->type) {
  case AST::EQ_EXP:
    type = IR::BEQ;
    break;
  case AST::GE_EXP:
    type = IR::BGE;
    break;
  case AST::GT_EXP:
    type = IR::BGT;
    break;
  case AST::LE_EXP:
    type = IR::BLE;
    break;
  case AST::LT_EXP:
    type = IR::BLT;
    break;
  case AST::NE_EXP:
    type = IR::BNE;
    break;
  default:
    break;
  }
  vector<IR *> irs3;
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  irs3.push_back(
      new IR(type, {new IRItem(trueLabel), lastResult(irs1)->clone(),
                    lastResult(irs2)->clone()}));
  irs3.push_back(new IR(IR::GOTO, {new IRItem(falseLabel)}));
  return irs3;
}

vector<IR *> IRParser::parseLOrExp(AST *root, Symbol *func, IR *trueLabel,
                                   IR *falseLabel) {
  IR *label = new IR(IR::LABEL);
  AST *left = root->nodes[0];
  AST *right = root->nodes[1];
  vector<IR *> irs1;
  switch (left->type) {
  case AST::ADD_EXP:
  case AST::DIV_EXP:
  case AST::FUNC_CALL:
  case AST::L_VAL:
  case AST::MOD_EXP:
  case AST::MUL_EXP:
  case AST::SUB_EXP:
    irs1 = parseAST(left, func);
    irs1.push_back(new IR(
        IR::BNE, {new IRItem(trueLabel), lastResult(irs1)->clone(),
                  lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    break;
  case AST::EQ_EXP:
  case AST::GE_EXP:
  case AST::GT_EXP:
  case AST::LE_EXP:
  case AST::LT_EXP:
  case AST::NE_EXP:
    irs1 = parseLCmpExp(left, func, trueLabel, label);
    break;
  case AST::L_AND_EXP:
    irs1 = parseLAndExp(left, func, trueLabel, label);
    break;
  case AST::L_OR_EXP:
    irs1 = parseLOrExp(left, func, trueLabel, label);
    break;
  case AST::FLOAT:
    irs1 = {new IR(IR::GOTO, {new IRItem(left->fVal ? trueLabel : label)})};
    break;
  case AST::INT:
    irs1 = {new IR(IR::GOTO, {new IRItem(root->iVal ? trueLabel : label)})};
    break;
  case AST::F2I_EXP:
    irs1 = parseAST(left, func);
    irs1.push_back(new IR(IR::BNE, {new IRItem(trueLabel),
                                    lastResult(irs1)->clone(), new IRItem(0)}));
    break;
  case AST::I2F_EXP:
    irs1 = parseAST(left->nodes[0], func);
    irs1.push_back(new IR(IR::BNE, {new IRItem(trueLabel),
                                    lastResult(irs1)->clone(), new IRItem(0)}));
    break;
  case AST::L_NOT_EXP:
    if (left->nodes[0]->type == AST::L_NOT_EXP) {
      irs1 = parseAST(left->nodes[0]->nodes[0], func);
      irs1.push_back(new IR(
          IR::BNE, {new IRItem(trueLabel), lastResult(irs1)->clone(),
                    lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                            : new IRItem(0)}));
      break;
    }
    irs1 = parseAST(left->nodes[0], func);
    irs1.push_back(new IR(
        IR::BEQ, {new IRItem(trueLabel), lastResult(irs1)->clone(),
                  lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    break;
  case AST::NEG_EXP:
    irs1 = parseAST(left->nodes[0], func);
    irs1.push_back(new IR(
        IR::BNE, {new IRItem(trueLabel), lastResult(irs1)->clone(),
                  lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    break;
  default:
    break;
  }
  vector<IR *> irs2;
  switch (right->type) {
  case AST::ADD_EXP:
  case AST::DIV_EXP:
  case AST::FUNC_CALL:
  case AST::L_VAL:
  case AST::MOD_EXP:
  case AST::MUL_EXP:
  case AST::SUB_EXP:
    irs2 = parseAST(right, func);
    irs2.push_back(new IR(
        IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                  lastResult(irs2)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
    break;
  case AST::EQ_EXP:
  case AST::GE_EXP:
  case AST::GT_EXP:
  case AST::LE_EXP:
  case AST::LT_EXP:
  case AST::NE_EXP:
    irs2 = parseLCmpExp(right, func, trueLabel, falseLabel);
    break;
  case AST::L_AND_EXP:
    irs2 = parseLAndExp(right, func, trueLabel, falseLabel);
    break;
  case AST::L_OR_EXP:
    irs2 = parseLOrExp(right, func, trueLabel, falseLabel);
    break;
  case AST::FLOAT:
    irs2 = {
        new IR(IR::GOTO, {new IRItem(right->fVal ? trueLabel : falseLabel)})};
    break;
  case AST::INT:
    irs2 = {
        new IR(IR::GOTO, {new IRItem(root->iVal ? trueLabel : falseLabel)})};
    break;
  case AST::F2I_EXP:
    irs2 = parseAST(right, func);
    irs2.push_back(new IR(IR::BEQ, {new IRItem(falseLabel),
                                    lastResult(irs2)->clone(), new IRItem(0)}));
    irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
    break;
  case AST::I2F_EXP:
    irs2 = parseAST(right->nodes[0], func);
    irs2.push_back(new IR(IR::BEQ, {new IRItem(falseLabel),
                                    lastResult(irs2)->clone(), new IRItem(0)}));
    irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
    break;
  case AST::L_NOT_EXP:
    if (right->nodes[0]->type == AST::L_NOT_EXP) {
      irs2 = parseAST(right->nodes[0]->nodes[0], func);
      irs2.push_back(new IR(
          IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                    lastResult(irs2)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                            : new IRItem(0)}));
      irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
      break;
    }
    irs2 = parseAST(right->nodes[0], func);
    irs2.push_back(new IR(
        IR::BNE, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                  lastResult(irs2)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
    break;
  case AST::NEG_EXP:
    irs2 = parseAST(right->nodes[0], func);
    irs2.push_back(new IR(
        IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                  lastResult(irs2)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
    break;
  default:
    break;
  }
  vector<IR *> irs3;
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.push_back(label);
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
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
  vector<IR *> irs;
  if (!root->nodes.empty()) {
    irs = parseAST(root->nodes[0], func);
    irs.push_back(new IR(
        IR::MOV, {new IRItem(IRItem::RETURN), lastResult(irs)->clone()}));
  }
  irs.push_back(new IR(IR::GOTO, {new IRItem(funcEnd)}));
  return irs;
}

void IRParser::parseRoot(AST *root) {
  this->isProcessed = true;
  for (AST *node : root->nodes) {
    switch (node->type) {
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

vector<IR *> IRParser::parseWhileStmt(AST *root, Symbol *func) {
  IR *label0 = new IR(IR::LABEL_WHILE_BEGIN);
  IR *label1 = new IR(IR::LABEL);
  IR *label2 = new IR(IR::LABEL_WHILE_END);
  vector<IR *> irs1 = parseCond(root->nodes[0], func, label1, label2);
  vector<IR *> irs2 = parseAST(root->nodes[1], func);
  vector<IR *> irs3 = parseCond(root->nodes[0], func, label1, label2);
  vector<IR *> irs4;
  irs4.push_back(label0);
  irs4.insert(irs4.end(), irs1.begin(), irs1.end());
  irs4.push_back(label1);
  irs4.insert(irs4.end(), irs2.begin(), irs2.end());
  irs4.insert(irs4.end(), irs3.begin(), irs3.end());
  irs4.push_back(label2);
  return irs4;
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