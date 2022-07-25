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

vector<IR *> IRParser::parseCond(AST *root, Symbol *func, IR *trueLabel,
                                 IR *falseLabel) {
  switch (root->astType) {
  case AST::BINARY_EXP:
    switch (root->opType) {
    case AST::ADD:
    case AST::DIV:
    case AST::MOD:
    case AST::MUL:
    case AST::SUB: {
      vector<IR *> irs = parseAST(root, func);
      irs.push_back(new IR(
          IR::BEQ, {new IRItem(falseLabel), lastResult(irs)->clone(),
                    lastResult(irs)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                           : new IRItem(0)}));
      return irs;
    }
    case AST::EQ:
    case AST::GE:
    case AST::GT:
    case AST::LE:
    case AST::LT:
    case AST::NE:
      return parseLCmpExp(root, func, trueLabel, falseLabel);
    case AST::L_AND:
      return parseLAndExp(root, func, trueLabel, falseLabel);
    case AST::L_OR:
      return parseLOrExp(root, func, trueLabel, falseLabel);
    default:
      break;
    }
    break;
  case AST::FLOAT:
    return {
        new IR(IR::GOTO, {new IRItem(root->fVal ? trueLabel : falseLabel)})};
  case AST::FUNC_CALL: {
    vector<IR *> irs = parseFuncCall(root, func);
    irs.push_back(new IR(
        IR::BEQ, {new IRItem(falseLabel), lastResult(irs)->clone(),
                  lastResult(irs)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                         : new IRItem(0)}));
    return irs;
  }
  case AST::INT:
    return {
        new IR(IR::GOTO, {new IRItem(root->iVal ? trueLabel : falseLabel)})};
  case AST::L_VAL: {
    vector<IR *> irs = parseRVal(root, func);
    irs.push_back(new IR(
        IR::BEQ, {new IRItem(falseLabel), lastResult(irs)->clone(),
                  lastResult(irs)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                         : new IRItem(0)}));
    return irs;
  }
  case AST::UNARY_EXP:
    switch (root->opType) {
    case AST::F2I: {
      vector<IR *> irs = parseUnaryExp(root, func);
      irs.push_back(new IR(IR::BEQ, {new IRItem(falseLabel),
                                     lastResult(irs)->clone(), new IRItem(0)}));
      return irs;
    }
    case AST::I2F: {
      vector<IR *> irs = parseAST(root->nodes[0], func);
      irs.push_back(new IR(IR::BEQ, {new IRItem(falseLabel),
                                     lastResult(irs)->clone(), new IRItem(0)}));
      return irs;
    }
    case AST::L_NOT: {
      if (root->nodes[0]->astType == AST::UNARY_EXP &&
          root->nodes[0]->opType == AST::L_NOT)
        return parseCond(root->nodes[0]->nodes[0], func, trueLabel, falseLabel);
      vector<IR *> irs = parseAST(root->nodes[0], func);
      irs.push_back(new IR(IR::BNE, {new IRItem(falseLabel),
                                     lastResult(irs)->clone(), new IRItem(0)}));
      return irs;
    }
    case AST::NEG:
      return parseCond(root->nodes[0], func, trueLabel, falseLabel);
    default:
      break;
    }
    break;
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
  for (AST *node : root->nodes) {
    vector<IR *> moreIRs = parseAST(node, func);
    irs.insert(irs.end(), moreIRs.begin(), moreIRs.end());
  }
  irs.push_back(new IR(IR::LABEL));
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
  } else
    irs4.push_back(label1);
  return irs4;
}

vector<IR *> IRParser::parseLAndExp(AST *root, Symbol *func, IR *trueLabel,
                                    IR *falseLabel) {
  IR *label = new IR(IR::LABEL);
  AST *left = root->nodes[0];
  AST *right = root->nodes[1];
  vector<IR *> irs1;
  switch (left->astType) {
  case AST::BINARY_EXP:
    switch (left->opType) {
    case AST::ADD:
    case AST::DIV:
    case AST::MOD:
    case AST::MUL:
    case AST::SUB:
      irs1 = parseAST(left, func);
      irs1.push_back(new IR(
          IR::BEQ, {new IRItem(falseLabel), lastResult(irs1)->clone(),
                    lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                            : new IRItem(0)}));
      break;
    case AST::EQ:
    case AST::GE:
    case AST::GT:
    case AST::LE:
    case AST::LT:
    case AST::NE:
      irs1 = parseLCmpExp(left, func, label, falseLabel);
      break;
    case AST::L_AND:
      irs1 = parseLAndExp(left, func, label, falseLabel);
      break;
    default:
      break;
    }
    break;
  case AST::FLOAT:
    irs1 = {new IR(IR::GOTO, {new IRItem(left->fVal ? label : falseLabel)})};
    break;
  case AST::FUNC_CALL:
    irs1 = parseFuncCall(left, func);
    irs1.push_back(new IR(
        IR::BEQ, {new IRItem(falseLabel), lastResult(irs1)->clone(),
                  lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    break;
  case AST::INT:
    irs1 = {new IR(IR::GOTO, {new IRItem(root->iVal ? label : falseLabel)})};
    break;
  case AST::L_VAL:
    irs1 = parseRVal(left, func);
    irs1.push_back(new IR(
        IR::BEQ, {new IRItem(falseLabel), lastResult(irs1)->clone(),
                  lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    break;
  case AST::UNARY_EXP:
    switch (left->opType) {
    case AST::F2I:
      irs1 = parseUnaryExp(left, func);
      irs1.push_back(
          new IR(IR::BEQ, {new IRItem(falseLabel), lastResult(irs1)->clone(),
                           new IRItem(0)}));
      break;
    case AST::I2F:
      irs1 = parseUnaryExp(left->nodes[0], func);
      irs1.push_back(
          new IR(IR::BEQ, {new IRItem(falseLabel), lastResult(irs1)->clone(),
                           new IRItem(0)}));
      break;
    case AST::L_NOT:
      if (left->nodes[0]->astType == AST::UNARY_EXP &&
          left->nodes[0]->opType == AST::L_NOT) {
        irs1 = parseAST(left->nodes[0]->nodes[0], func);
        irs1.push_back(
            new IR(IR::BEQ,
                   {new IRItem(falseLabel), lastResult(irs1)->clone(),
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
    case AST::NEG:
      irs1 = parseAST(left->nodes[0], func);
      irs1.push_back(new IR(
          IR::BEQ, {new IRItem(falseLabel), lastResult(irs1)->clone(),
                    lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                            : new IRItem(0)}));
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
  vector<IR *> irs2;
  switch (right->astType) {
  case AST::BINARY_EXP:
    switch (right->opType) {
    case AST::ADD:
    case AST::DIV:
    case AST::MOD:
    case AST::MUL:
    case AST::SUB:
      irs2 = parseAST(right, func);
      irs2.push_back(new IR(
          IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                    lastResult(irs2)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                            : new IRItem(0)}));
      irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
      break;
    case AST::EQ:
    case AST::GE:
    case AST::GT:
    case AST::LE:
    case AST::LT:
    case AST::NE:
      irs2 = parseLCmpExp(right, func, trueLabel, falseLabel);
      break;
    case AST::L_AND:
      irs2 = parseLAndExp(right, func, trueLabel, falseLabel);
      break;
    default:
      break;
    }
    break;
  case AST::FLOAT:
    irs2 = {
        new IR(IR::GOTO, {new IRItem(right->fVal ? trueLabel : falseLabel)})};
    break;
  case AST::FUNC_CALL:
    irs2 = parseFuncCall(right, func);
    irs2.push_back(new IR(
        IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                  lastResult(irs2)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
    break;
  case AST::INT:
    irs2 = {
        new IR(IR::GOTO, {new IRItem(root->iVal ? trueLabel : falseLabel)})};
    break;
  case AST::L_VAL:
    irs2 = parseRVal(right, func);
    irs2.push_back(new IR(
        IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                  lastResult(irs2)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
    break;
  case AST::UNARY_EXP:
    switch (right->opType) {
    case AST::F2I:
      irs2 = parseUnaryExp(right, func);
      irs2.push_back(
          new IR(IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                           new IRItem(0)}));
      irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
      break;
    case AST::I2F:
      irs2 = parseUnaryExp(right->nodes[0], func);
      irs2.push_back(
          new IR(IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                           new IRItem(0)}));
      irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
      break;
    case AST::L_NOT:
      if (right->nodes[0]->astType == AST::UNARY_EXP &&
          right->nodes[0]->opType == AST::L_NOT) {
        irs2 = parseAST(right->nodes[0]->nodes[0], func);
        irs2.push_back(
            new IR(IR::BEQ,
                   {new IRItem(falseLabel), lastResult(irs2)->clone(),
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
    case AST::NEG:
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
  switch (left->astType) {
  case AST::BINARY_EXP:
    switch (left->opType) {
    case AST::ADD:
    case AST::DIV:
    case AST::MOD:
    case AST::MUL:
    case AST::SUB:
      irs1 = parseAST(left, func);
      break;
    case AST::EQ:
    case AST::GE:
    case AST::GT:
    case AST::LE:
    case AST::LT:
    case AST::NE:
      irs1 = parseCmpExp(left, func);
      break;
    default:
      break;
    }
    break;
  case AST::FLOAT:
    irs1 = {new IR(IR::MOV, {new IRItem(IRItem::FTEMP, tempId++),
                             new IRItem(left->fVal)})};
    break;
  case AST::FUNC_CALL:
    irs1 = parseFuncCall(left, func);
    break;
  case AST::INT:
    irs1 = {new IR(IR::MOV, {new IRItem(IRItem::ITEMP, tempId++),
                             new IRItem(left->iVal)})};
    break;
  case AST::L_VAL:
    irs1 = parseRVal(left, func);
    break;
  case AST::UNARY_EXP:
    irs1 = parseUnaryExp(left, func);
    break;
  default:
    break;
  }
  vector<IR *> irs2;
  switch (right->astType) {
  case AST::BINARY_EXP:
    switch (right->opType) {
    case AST::ADD:
    case AST::DIV:
    case AST::MOD:
    case AST::MUL:
    case AST::SUB:
      irs2 = parseAST(right, func);
      break;
    case AST::EQ:
    case AST::GE:
    case AST::GT:
    case AST::LE:
    case AST::LT:
    case AST::NE:
      irs2 = parseCmpExp(right, func);
      break;
    default:
      break;
    }
    break;
  case AST::FLOAT:
    irs2 = {new IR(IR::MOV, {new IRItem(IRItem::FTEMP, tempId++),
                             new IRItem(right->fVal)})};
    break;
  case AST::FUNC_CALL:
    irs2 = parseFuncCall(right, func);
    break;
  case AST::INT:
    irs2 = {new IR(IR::MOV, {new IRItem(IRItem::ITEMP, tempId++),
                             new IRItem(right->iVal)})};
    break;
  case AST::L_VAL:
    irs2 = parseRVal(right, func);
    break;
  case AST::UNARY_EXP:
    irs2 = parseUnaryExp(right, func);
    break;
  default:
    break;
  }
  IR::IRType type = IR::BEQ;
  switch (root->opType) {
  case AST::EQ:
    type = IR::BNE;
    break;
  case AST::GE:
    type = IR::BLT;
    break;
  case AST::GT:
    type = IR::BLE;
    break;
  case AST::LE:
    type = IR::BGT;
    break;
  case AST::LT:
    type = IR::BGE;
    break;
  case AST::NE:
    type = IR::BEQ;
    break;
  default:
    break;
  }
  vector<IR *> irs3;
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  irs3.push_back(
      new IR(type, {new IRItem(falseLabel), lastResult(irs1)->clone(),
                    lastResult(irs2)->clone()}));
  irs3.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
  return irs3;
}

vector<IR *> IRParser::parseLOrExp(AST *root, Symbol *func, IR *trueLabel,
                                   IR *falseLabel) {
  IR *label = new IR(IR::LABEL);
  AST *left = root->nodes[0];
  AST *right = root->nodes[1];
  vector<IR *> irs1;
  switch (left->astType) {
  case AST::BINARY_EXP:
    switch (left->opType) {
    case AST::ADD:
    case AST::DIV:
    case AST::MOD:
    case AST::MUL:
    case AST::SUB:
      irs1 = parseAST(left, func);
      irs1.push_back(new IR(
          IR::BNE, {new IRItem(trueLabel), lastResult(irs1)->clone(),
                    lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                            : new IRItem(0)}));
      break;
    case AST::EQ:
    case AST::GE:
    case AST::GT:
    case AST::LE:
    case AST::LT:
    case AST::NE:
      irs1 = parseLCmpExp(left, func, trueLabel, label);
      break;
    case AST::L_AND:
      irs1 = parseLAndExp(left, func, trueLabel, label);
      break;
    case AST::L_OR:
      irs1 = parseLOrExp(left, func, trueLabel, label);
      break;
    default:
      break;
    }
    break;
  case AST::FLOAT:
    irs1 = {new IR(IR::GOTO, {new IRItem(left->fVal ? trueLabel : label)})};
    break;
  case AST::FUNC_CALL:
    irs1 = parseFuncCall(left, func);
    irs1.push_back(new IR(
        IR::BNE, {new IRItem(trueLabel), lastResult(irs1)->clone(),
                  lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    break;
  case AST::INT:
    irs1 = {new IR(IR::GOTO, {new IRItem(root->iVal ? trueLabel : label)})};
    break;
  case AST::L_VAL:
    irs1 = parseRVal(left, func);
    irs1.push_back(new IR(
        IR::BNE, {new IRItem(trueLabel), lastResult(irs1)->clone(),
                  lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    break;
  case AST::UNARY_EXP:
    switch (left->opType) {
    case AST::F2I:
      irs1 = parseUnaryExp(left, func);
      irs1.push_back(
          new IR(IR::BNE, {new IRItem(trueLabel), lastResult(irs1)->clone(),
                           new IRItem(0)}));
      break;
    case AST::I2F:
      irs1 = parseUnaryExp(left->nodes[0], func);
      irs1.push_back(
          new IR(IR::BNE, {new IRItem(trueLabel), lastResult(irs1)->clone(),
                           new IRItem(0)}));
      break;
    case AST::L_NOT:
      if (left->nodes[0]->astType == AST::UNARY_EXP &&
          left->nodes[0]->opType == AST::L_NOT) {
        irs1 = parseAST(left->nodes[0]->nodes[0], func);
        irs1.push_back(
            new IR(IR::BNE,
                   {new IRItem(trueLabel), lastResult(irs1)->clone(),
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
    case AST::NEG:
      irs1 = parseAST(left->nodes[0], func);
      irs1.push_back(new IR(
          IR::BNE, {new IRItem(trueLabel), lastResult(irs1)->clone(),
                    lastResult(irs1)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                            : new IRItem(0)}));
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
  vector<IR *> irs2;
  switch (right->astType) {
  case AST::BINARY_EXP:
    switch (right->opType) {
    case AST::ADD:
    case AST::DIV:
    case AST::MOD:
    case AST::MUL:
    case AST::SUB:
      irs2 = parseAST(right, func);
      irs2.push_back(new IR(
          IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                    lastResult(irs2)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                            : new IRItem(0)}));
      irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
      break;
    case AST::EQ:
    case AST::GE:
    case AST::GT:
    case AST::LE:
    case AST::LT:
    case AST::NE:
      irs2 = parseLCmpExp(right, func, trueLabel, falseLabel);
      break;
    case AST::L_AND:
      irs2 = parseLAndExp(right, func, trueLabel, falseLabel);
      break;
    case AST::L_OR:
      irs2 = parseLOrExp(right, func, trueLabel, falseLabel);
      break;
    default:
      break;
    }
    break;
  case AST::FLOAT:
    irs2 = {
        new IR(IR::GOTO, {new IRItem(right->fVal ? trueLabel : falseLabel)})};
    break;
  case AST::FUNC_CALL:
    irs2 = parseFuncCall(right, func);
    irs2.push_back(new IR(
        IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                  lastResult(irs2)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
    break;
  case AST::INT:
    irs2 = {
        new IR(IR::GOTO, {new IRItem(root->iVal ? trueLabel : falseLabel)})};
    break;
  case AST::L_VAL:
    irs2 = parseRVal(right, func);
    irs2.push_back(new IR(
        IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                  lastResult(irs2)->type == IRItem::FTEMP ? new IRItem(0.0f)
                                                          : new IRItem(0)}));
    irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
    break;
  case AST::UNARY_EXP:
    switch (right->opType) {
    case AST::F2I:
      irs2 = parseUnaryExp(right, func);
      irs2.push_back(
          new IR(IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                           new IRItem(0)}));
      irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
      break;
    case AST::I2F:
      irs2 = parseUnaryExp(right->nodes[0], func);
      irs2.push_back(
          new IR(IR::BEQ, {new IRItem(falseLabel), lastResult(irs2)->clone(),
                           new IRItem(0)}));
      irs2.push_back(new IR(IR::GOTO, {new IRItem(trueLabel)}));
      break;
    case AST::L_NOT:
      if (right->nodes[0]->astType == AST::UNARY_EXP &&
          right->nodes[0]->opType == AST::L_NOT) {
        irs2 = parseAST(right->nodes[0]->nodes[0], func);
        irs2.push_back(
            new IR(IR::BEQ,
                   {new IRItem(falseLabel), lastResult(irs2)->clone(),
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
    case AST::NEG:
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
  IR *label0 = new IR(IR::LABEL_WHILE_BEGIN);
  IR *label1 = new IR(IR::LABEL);
  IR *label2 = new IR(IR::LABEL_WHILE_END);
  vector<IR *> irs1 = parseCond(root->nodes[0], func, label1, label2);
  vector<IR *> irs2 = parseAST(root->nodes[1], func);
  vector<IR *> irs3;
  irs3.push_back(label0);
  irs3.insert(irs3.end(), irs1.begin(), irs1.end());
  irs3.push_back(label1);
  irs3.insert(irs3.end(), irs2.begin(), irs2.end());
  irs3.push_back(new IR(IR::GOTO, {new IRItem(label0)}));
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