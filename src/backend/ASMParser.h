#ifndef __ASM_PARSER_H__
#define __ASM_PARSER_H__

#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "../frontend/IR.h"
#include "../frontend/Symbol.h"
#include "ASM.h"

using namespace std;

class ASMParser {
public:
  int labelId;
  string asmFile;
  vector<Symbol *> consts;
  vector<Symbol *> globalVars;
  unordered_map<Symbol *, vector<Symbol *>> localVars;
  vector<pair<Symbol *, vector<IR *>>> funcIRs;
  unordered_map<IR *, string> irLabels;
  vector<pair<Symbol *, vector<ASM *>>> funcASMs;
  unordered_map<ASMItem::RegType, unsigned> reg2Temp;
  unordered_map<unsigned, ASMItem::RegType> temp2Reg;

  unordered_map<int, unsigned> allocReg(const vector<IR *> &);
  ASMItem::RegType getReg(int);
  vector<ASM *> parseFunc(Symbol *, const vector<IR *> &);
  void parse();
  void preProcess();
  void writeASMFile();

  ASMParser(string &, vector<pair<Symbol *, vector<IR *>>> &,
            vector<Symbol *> &, vector<Symbol *> &,
            unordered_map<Symbol *, vector<Symbol *>> &);
  ~ASMParser();
};

#endif