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
  unordered_map<unsigned, ASMItem::RegType> ftemp2Reg;
  unordered_map<unsigned, ASMItem::RegType> itemp2Reg;
  unsigned spillSize;
  unordered_map<unsigned, int> temp2SpillReg;
  unordered_map<unsigned, int> spillOffsets;
  unordered_map<Symbol *, int> offsets;
  int startLineno;
  int stopLineno;

  pair<int, int> allocReg(const vector<IR *> &);
  int calcArgs(const vector<IR *> &);
  void parse();
  void parseAdd(vector<ASM *> &, IR *);
  void parseArg(vector<ASM *> &, IR *);
  void parseB(vector<ASM *> &, IR *);
  void parseCmp(vector<ASM *> &, IR *);
  void parseDiv(vector<ASM *> &, IR *);
  void parseF2I(vector<ASM *> &, IR *);
  vector<ASM *> parseFunc(Symbol *, const vector<IR *> &);
  void parseI2F(vector<ASM *> &, IR *);
  void parseLNot(vector<ASM *> &, IR *);
  void parseLoad(vector<ASM *> &, IR *);
  void parseMod(vector<ASM *> &, IR *);
  void parseMov(vector<ASM *> &, IR *);
  void parseMul(vector<ASM *> &, IR *);
  void parseNeg(vector<ASM *> &, IR *);
  void parseSub(vector<ASM *> &, IR *);
  void preProcess();
  void removeUnusedLabels(vector<ASM *> &);
  void writeASMFile();

  ASMParser(const string &, pair<unsigned, unsigned>,
            vector<pair<Symbol *, vector<IR *>>> &, vector<Symbol *> &,
            vector<Symbol *> &, unordered_map<Symbol *, vector<Symbol *>> &);
  ~ASMParser();
};

#endif