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
  int irId;
  string asmFile;
  vector<Symbol *> consts;
  vector<Symbol *> globalVars;
  unordered_map<Symbol *, vector<Symbol *>> localVars;
  vector<pair<Symbol *, vector<IR *>>> funcIRs;
  unordered_map<IR *, string> irLabels;
  vector<pair<Symbol *, vector<ASM *>>> funcASMs;
  unordered_map<ASMItem::RegType, unsigned> reg2Temp;
  unordered_map<unsigned, ASMItem::RegType> temp2Reg;
  unordered_map<Symbol *, int> offsets;
  unordered_map<int, unsigned> regEnds;

  void allocReg(const vector<IR *> &);
  ASMItem::RegType getSReg(int);
  ASMItem::RegType getVReg(int);
  void parse();
  void parseAdd(vector<ASM *> &, IR *);
  void parseB(vector<ASM *> &, IR *);
  void parseCmp(vector<ASM *> &, IR *);
  void parseDiv(vector<ASM *> &, IR *);
  vector<ASM *> parseFunc(Symbol *, const vector<IR *> &);
  void parseLNot(vector<ASM *> &, IR *);
  void parseMod(vector<ASM *> &, IR *);
  void parseMov(vector<ASM *> &, IR *, IR *);
  void parseMul(vector<ASM *> &, IR *);
  void parseNeg(vector<ASM *> &, IR *);
  void parseSub(vector<ASM *> &, IR *);
  ASMItem::RegType popSReg();
  ASMItem::RegType popVReg();
  void preProcess();
  void pushReg(ASMItem::RegType);
  void removeUnusedLabels(vector<ASM *> &);
  void writeASMFile();

  ASMParser(string &, vector<pair<Symbol *, vector<IR *>>> &,
            vector<Symbol *> &, vector<Symbol *> &,
            unordered_map<Symbol *, vector<Symbol *>> &);
  ~ASMParser();
};

#endif