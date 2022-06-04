#include <iostream>

#include "AST.h"

using namespace std;

AST::AST(ASTType astType) {
  this->symbol = nullptr;
  this->astType = astType;
}

AST::AST(ASTType astType, const vector<AST *> &nodes) {
  this->symbol = nullptr;
  this->astType = astType;
  this->nodes = nodes;
}

AST::AST(ASTType astType, OPType opType) {
  this->symbol = nullptr;
  this->astType = astType;
  this->opType = opType;
}

AST::AST(ASTType astType, OPType opType, const vector<AST *> &nodes) {
  this->symbol = nullptr;
  this->astType = astType;
  this->opType = opType;
  this->nodes = nodes;
}

AST::AST(ASTType astType, Symbol *symbol) {
  this->astType = astType;
  this->symbol = symbol;
}

AST::AST(ASTType astType, Symbol *symbol, const vector<AST *> &nodes) {
  this->astType = astType;
  this->symbol = symbol;
  this->nodes = nodes;
}

AST::AST(ASTType astType, const string &name, const vector<AST *> &nodes) {
  this->symbol = nullptr;
  this->astType = astType;
  this->name = name;
  this->nodes = nodes;
}

AST::AST(float fVal) {
  this->symbol = nullptr;
  this->astType = FLOAT_LITERAL;
  this->fVal = fVal;
}

AST::AST(int iVal) {
  this->symbol = nullptr;
  this->astType = INT_LITERAL;
  this->iVal = iVal;
}

AST::~AST() {
  for (AST *node : nodes)
    delete node;
}

void AST::print(int depth) {
  bool first = true;
  bool flag = false;
  switch (astType) {
  case ASSIGN_STMT:
    cout << string(depth << 1, ' ');
    nodes[0]->print(depth);
    cout << " = ";
    nodes[1]->print(depth);
    cout << ";" << endl;
    break;
  case BINARY_EXP:
    cout << "(";
    nodes[0]->print(depth);
    switch (opType) {
    case ADD:
      cout << " + ";
      break;
    case DIV:
      cout << " / ";
      break;
    case EQ:
      cout << " == ";
      break;
    case GE:
      cout << " >= ";
      break;
    case GT:
      cout << " > ";
      break;
    case LE:
      cout << " <= ";
      break;
    case LT:
      cout << " < ";
      break;
    case L_AND:
      cout << " && ";
      break;
    case L_OR:
      cout << " || ";
      break;
    case MOD:
      cout << " % ";
      break;
    case MUL:
      cout << " * ";
      break;
    case NE:
      cout << " != ";
      break;
    case SUB:
      cout << " - ";
      break;
    default:
      break;
    }
    nodes[1]->print(depth);
    cout << ")";
    break;
  case BLANK_STMT:
    cout << string(depth << 1, ' ') << ";" << endl;
    break;
  case BLOCK:
    cout << string(depth << 1, ' ') << "{" << endl;
    for (AST *node : nodes)
      node->print(depth + 1);
    cout << string(depth << 1, ' ') << "}" << endl;
    break;
  case BREAK_STMT:
    cout << string(depth << 1, ' ') << "break;" << endl;
    break;
  case CONST_DEF:
    cout << string(depth << 1, ' ') << "const "
         << (symbol->dataType == Symbol::INT ? "int" : "float") << " "
         << symbol->name;
    for (int dimension : symbol->dimensions) {
      cout << "[" << dimension << "]";
      flag = true;
    }
    cout << " = ";
    if (flag) {
      cout << "{";
      first = true;
      if (symbol->dataType == Symbol::INT) {
        for (pair<int, int> iPair : symbol->iMap) {
          if (!first)
            cout << ", ";
          cout << "{" << iPair.first << ", " << iPair.second << "}";
          first = false;
        }
      } else {
        for (pair<int, float> fPair : symbol->fMap) {
          if (!first)
            cout << ", ";
          cout << "{" << fPair.first << ", " << fPair.second << "}";
          first = false;
        }
      }
      cout << "}";
    } else {
      if (symbol->dataType == Symbol::INT)
        cout << symbol->iVal;
      else
        cout << symbol->fVal;
    }
    cout << ";" << endl;
    break;
  case GLOBAL_VAR_DEF:
    cout << string(depth << 1, ' ')
         << (symbol->dataType == Symbol::INT ? "int" : "float") << " "
         << symbol->name;
    for (int dimension : symbol->dimensions) {
      cout << "[" << dimension << "]";
      flag = true;
    }
    cout << " = ";
    if (flag) {
      cout << "{";
      first = true;
      if (symbol->dataType == Symbol::INT) {
        for (pair<int, int> iPair : symbol->iMap) {
          if (!first)
            cout << ", ";
          cout << "{" << iPair.first << ", " << iPair.second << "}";
          first = false;
        }
      } else {
        for (pair<int, float> fPair : symbol->fMap) {
          if (!first)
            cout << ", ";
          cout << "{" << fPair.first << ", " << fPair.second << "}";
          first = false;
        }
      }
      cout << "}";
    } else {
      if (symbol->dataType == Symbol::INT)
        cout << symbol->iVal;
      else
        cout << symbol->fVal;
    }
    cout << ";" << endl;
    break;
  case CONTINUE_STMT:
    cout << string(depth << 1, ' ') << "continue;" << endl;
    break;
  case EXP_STMT:
    cout << string(depth << 1, ' ');
    nodes[0]->print(depth);
    cout << ";" << endl;
    break;
  case FLOAT_LITERAL:
    cout << fVal;
    break;
  case FUNC_CALL: {
    cout << (symbol ? symbol->name : name) << "(";
    for (AST *node : nodes) {
      if (!first)
        cout << ", ";
      node->print(depth);
      first = false;
    }
    cout << ")";
    break;
  }
  case FUNC_DEF: {
    switch (symbol->dataType) {
    case Symbol::FLOAT:
      cout << "float";
      break;
    case Symbol::INT:
      cout << "int";
      break;
    case Symbol::VOID:
      cout << "void";
      break;
    default:
      break;
    }
    cout << " " << symbol->name << "(";
    for (unsigned i = 0; i < symbol->params.size(); i++) {
      if (!first)
        cout << ", ";
      cout << (symbol->params[i]->dataType == Symbol::INT ? "int" : "float")
           << " " << name;
      for (unsigned j = 0; j < symbol->params[i]->dimensions.size(); j++) {
        cout << "[";
        if (symbol->params[i]->dimensions[j] != -1)
          cout << symbol->params[i]->dimensions[j];
        cout << "]";
      }
      first = false;
    }
    cout << ")" << endl;
    nodes.back()->print(depth);
    break;
  }
  case IF_STMT:
    cout << string(depth << 1, ' ') << "if (";
    nodes[0]->print(depth);
    cout << ")" << endl;
    nodes[1]->print(depth + (nodes[1]->astType != BLOCK));
    if (nodes[2]) {
      cout << string(depth << 1, ' ') << "else" << endl;
      nodes[2]->print(depth + (nodes[2]->astType != BLOCK));
    }
    break;
  case INT_LITERAL:
    cout << iVal;
    break;
  case L_VAL:
    cout << (symbol ? symbol->name : name);
    for (AST *node : nodes) {
      cout << "[";
      node->print(depth);
      cout << "]";
    }
    break;
  case MEMSET_ZERO:
    cout << string(depth << 1, ' ') << "memset(" << symbol->name
         << ", 0, sizeof(" << symbol->name << ");" << endl;
    break;
  case RETURN_STMT:
    cout << string(depth << 1, ' ') << "return ";
    if (!nodes.empty())
      nodes[0]->print(depth + 1);
    cout << ";" << endl;
    break;
  case ROOT:
    for (AST *node : nodes)
      node->print(depth);
    break;
  case UNARY_EXP:
    switch (opType) {
    case L_NOT:
      cout << "!";
      break;
    case NEG:
      cout << "-";
      break;
    case POS:
      cout << "+";
      break;
    default:
      break;
    }
    nodes[0]->print(depth);
    break;
  case LOCAL_VAR_DEF:
    cout << string(depth << 1, ' ')
         << (symbol->dataType == Symbol::INT ? "int" : "float") << " "
         << symbol->name;
    for (int dimension : symbol->dimensions)
      cout << "[" << dimension << "]";
    cout << ";" << endl;
    break;
  case WHILE_STMT:
    cout << string(depth << 1, ' ') << "while (";
    nodes[0]->print(depth);
    cout << ")" << endl;
    nodes[1]->print(depth + (nodes[1]->astType != BLOCK));
    break;
  default:
    break;
  }
}