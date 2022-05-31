#include <iostream>

#include "AST.h"

using namespace std;

AST::AST(Type astType) {
  this->symbol = nullptr;
  this->astType = astType;
}

AST::AST(Type astType, AST *node) {
  this->symbol = nullptr;
  this->astType = astType;
  this->type = type;
  this->nodes.push_back(node);
}

AST::AST(Type astType, AST *left, AST *right) {
  this->symbol = nullptr;
  this->astType = astType;
  this->nodes.push_back(left);
  this->nodes.push_back(right);
}

AST::AST(Type astType, AST *left, AST *mid, AST *right) {
  this->symbol = nullptr;
  this->astType = astType;
  this->nodes.push_back(left);
  this->nodes.push_back(mid);
  this->nodes.push_back(right);
}

AST::AST(Type astType, Symbol *symbol) {
  this->astType = astType;
  this->symbol = symbol;
}

AST::AST(Type astType, Symbol *symbol, AST *node) {
  this->astType = astType;
  this->symbol = symbol;
  this->nodes.push_back(node);
}

AST::AST(Type astType, Symbol *symbol, vector<AST *> nodes) {
  this->astType = astType;
  this->symbol = symbol;
  this->nodes = nodes;
}

AST::AST(Type astType, Type type) {
  this->symbol = nullptr;
  this->astType = astType;
  this->type = type;
}

AST::AST(Type astType, Type type, AST *node) {
  this->symbol = nullptr;
  this->astType = astType;
  this->type = type;
  this->nodes.push_back(node);
}

AST::AST(Type astType, Type type, AST *left, AST *right) {
  this->symbol = nullptr;
  this->astType = astType;
  this->type = type;
  this->nodes.push_back(left);
  this->nodes.push_back(right);
}

AST::AST(Type astType, Type type, string name) {
  this->symbol = nullptr;
  this->astType = astType;
  this->type = type;
  this->name = name;
}

AST::AST(Type astType, Type type, string name, vector<AST *> nodes) {
  this->symbol = nullptr;
  this->astType = astType;
  this->type = type;
  this->name = name;
  this->nodes = nodes;
}

AST::AST(Type astType, Type type, string name, vector<AST *> nodes, AST *node) {
  this->symbol = nullptr;
  this->astType = astType;
  this->type = type;
  this->name = name;
  this->nodes.insert(this->nodes.end(), nodes.begin(), nodes.end());
  this->nodes.push_back(node);
}

AST::AST(Type astType, Type type, vector<AST *> nodes) {
  this->symbol = nullptr;
  this->astType = astType;
  this->type = type;
  this->nodes = nodes;
}

AST::AST(Type astType, string name, vector<AST *> nodes) {
  this->symbol = nullptr;
  this->astType = astType;
  this->name = name;
  this->nodes = nodes;
}

AST::AST(Type astType, vector<AST *> nodes) {
  this->symbol = nullptr;
  this->astType = astType;
  this->nodes = nodes;
}

AST::AST(Type astType, vector<AST *> nodes, vector<Type> types) {
  this->symbol = nullptr;
  this->astType = astType;
  this->nodes = nodes;
  this->ops = types;
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

AST::~AST() {}

void AST::print(int depth) {
  bool first = true;
  bool flag = false;
  int size = 0;
  int count = 0;
  switch (astType) {
  case ASSIGN_STMT:
    for (int i = 0; i < depth; i++)
      cout << "  ";
    nodes[0]->print(depth);
    cout << " = ";
    nodes[1]->print(depth);
    cout << ";" << endl;
    break;
  case BINARY_EXP:
    cout << "(";
    nodes[0]->print(depth);
    switch (type) {
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
    case MINUS:
      cout << " - ";
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
    case PLUS:
      cout << " + ";
      break;
    default:
      break;
    }
    nodes[1]->print(depth);
    cout << ")";
    break;
  case BLANK_STMT:
    for (int i = 0; i < depth; i++)
      cout << "  ";
    cout << ";" << endl;
    break;
  case BLOCK:
    for (int i = 0; i < depth; i++)
      cout << "  ";
    cout << "{" << endl;
    for (AST *node : nodes)
      node->print(depth + 1);
    for (int i = 0; i < depth; i++)
      cout << "  ";
    cout << "}" << endl;
    break;
  case BREAK_STMT:
    for (int i = 0; i < depth; i++)
      cout << "  ";
    cout << "break;" << endl;
    break;
  case CONST_DEF:
    for (int i = 0; i < depth; i++)
      cout << "  ";
    cout << "const ";
    switch (symbol->dataType) {
    case FLOAT:
      cout << "float";
      break;
    case INT:
      cout << "int";
      break;
    default:
      break;
    }
    cout << " ";
    cout << symbol->name;
    size = 1;
    for (int dimension : symbol->dimensions) {
      cout << "[" << dimension << "]";
      size *= dimension;
      flag = true;
    }
    cout << " = ";
    switch (symbol->dataType) {
    case FLOAT:
      if (flag) {
        cout << "{";
        first = true;
        for (int i = 0; i < size; i++) {
          if (symbol->floatArray[i] == 0) {
            count++;
            continue;
          }
          if (count > 0) {
            if (!first)
              cout << ", ";
            cout << count << " ** " << 0.0;
            first = false;
            count = 0;
          }
          if (!first)
            cout << ", ";
          cout << symbol->floatArray[i];
          first = false;
        }
        if (count > 0) {
          if (!first)
            cout << ", ";
          cout << count << " ** " << 0.0;
        }
        cout << "}";
      } else
        cout << symbol->floatValue;
      break;
    case INT:
      if (flag) {
        cout << "{";
        first = true;
        for (int i = 0; i < size; i++) {
          if (symbol->intArray[i] == 0) {
            count++;
            continue;
          }
          if (count > 0) {
            if (!first)
              cout << ", ";
            cout << count << " ** " << 0;
            first = false;
            count = 0;
          }
          if (!first)
            cout << ", ";
          cout << symbol->intArray[i];
          first = false;
        }
        if (count > 0) {
          if (!first)
            cout << ", ";
          cout << count << " ** " << 0;
        }
        cout << "}";
      } else
        cout << symbol->intValue;
      break;
    default:
      break;
    }
    cout << ";" << endl;
    break;
  case GLOBAL_VAR_DEF:
    for (int i = 0; i < depth; i++)
      cout << "  ";
    switch (symbol->dataType) {
    case FLOAT:
      cout << "float";
      break;
    case INT:
      cout << "int";
      break;
    default:
      break;
    }
    cout << " ";
    cout << symbol->name;
    size = 1;
    for (int dimension : symbol->dimensions) {
      cout << "[" << dimension << "]";
      size *= dimension;
      flag = true;
    }
    cout << " = ";
    switch (symbol->dataType) {
    case FLOAT:
      if (flag) {
        cout << "{";
        first = true;
        for (int i = 0; i < size; i++) {
          if (symbol->floatArray[i] == 0) {
            count++;
            continue;
          }
          if (count > 0) {
            if (!first)
              cout << ", ";
            cout << count << " ** " << 0.0;
            first = false;
            count = 0;
          }
          if (!first)
            cout << ", ";
          cout << symbol->floatArray[i];
          first = false;
        }
        if (count > 0) {
          if (!first)
            cout << ", ";
          cout << count << " ** " << 0.0;
        }
        cout << "}";
      } else
        cout << symbol->floatValue;
      break;
    case INT:
      if (flag) {
        cout << "{";
        first = true;
        for (int i = 0; i < size; i++) {
          if (symbol->intArray[i] == 0) {
            count++;
            continue;
          }
          if (count > 0) {
            if (!first)
              cout << ", ";
            cout << count << " ** " << 0;
            first = false;
            count = 0;
          }
          if (!first)
            cout << ", ";
          cout << symbol->intArray[i];
          first = false;
        }
        if (count > 0) {
          if (!first)
            cout << ", ";
          cout << count << " ** " << 0;
        }
        cout << "}";
      } else
        cout << symbol->intValue;
      break;
    default:
      break;
    }
    cout << ";" << endl;
    break;
  case CONTINUE_STMT:
    for (int i = 0; i < depth; i++)
      cout << "  ";
    cout << "continue;" << endl;
    break;
  case EXP_STMT:
    for (int i = 0; i < depth; i++)
      cout << "  ";
    nodes.front()->print(depth);
    cout << ";" << endl;
    break;
  case FLOAT_LITERAL:
    cout << fVal;
    break;
  case FUNC_CALL: {
    if (symbol)
      cout << symbol->name;
    else
      cout << name;
    cout << "(";
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
    case FLOAT:
      cout << "float";
      break;
    case INT:
      cout << "int";
      break;
    case VOID:
      cout << "void";
      break;
    default:
      break;
    }
    cout << " " << symbol->name << "(";
    for (unsigned i = 0; i < symbol->params.size(); i++) {
      if (!first)
        cout << ", ";
      switch (symbol->params[i]->dataType) {
      case FLOAT:
        cout << "float";
        break;
      case INT:
        cout << "int";
        break;
      default:
        break;
      }
      cout << " " << name;
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
    for (int i = 0; i < depth; i++)
      cout << "  ";
    cout << "if (";
    nodes[0]->print(depth);
    cout << ")" << endl;
    nodes[1]->print(depth + (nodes[1]->astType != BLOCK));
    if (nodes[2]) {
      for (int i = 0; i < depth; i++)
        cout << "  ";
      cout << "else" << endl;
      nodes[2]->print(depth + (nodes[2]->astType != BLOCK));
    }
    break;
  case INIT_VAL:
    if (type == VAL) {
      nodes.front()->print(depth);
      return;
    }
    cout << "{";
    for (AST *node : nodes) {
      if (!first)
        cout << ", ";
      node->print(depth);
      first = false;
    }
    cout << "}";
    break;
  case INT_LITERAL:
    cout << iVal;
    break;
  case L_VAL:
    if (symbol)
      cout << symbol->name;
    else
      cout << name;
    for (AST *node : nodes) {
      cout << "[";
      node->print(depth);
      cout << "]";
    }
    break;
  case RETURN_STMT:
    for (int i = 0; i < depth; i++)
      cout << "  ";
    cout << "return ";
    if (!nodes.empty())
      nodes.front()->print(depth + 1);
    cout << ";" << endl;
    break;
  case ROOT:
    for (AST *node : nodes)
      node->print(depth);
    break;
  case UNARY_EXP:
    switch (type) {
    case L_NOT:
      cout << "!";
      break;
    case MINUS:
      cout << "-";
      break;
    case PLUS:
      cout << "+";
      break;
    default:
      break;
    }
    nodes.front()->print(depth);
    break;
  case LOCAL_VAR_DEF:
    for (int i = 0; i < depth; i++)
      cout << "  ";
    switch (symbol->dataType) {
    case FLOAT:
      cout << "float";
      break;
    case INT:
      cout << "int";
      break;
    default:
      break;
    }
    cout << " ";
    cout << symbol->name;
    for (int dimension : symbol->dimensions)
      cout << "[" << dimension << "]";
    cout << ";" << endl;
    break;
  case WHILE_STMT:
    for (int i = 0; i < depth; i++)
      cout << "  ";
    cout << "while (";
    nodes[0]->print(depth);
    cout << ")" << endl;
    nodes[1]->print(depth + (nodes[1]->astType != BLOCK));
    break;
  default:
    break;
  }
}