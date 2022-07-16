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
private:
  const vector<ASMItem::RegType> aIRegs = {ASMItem::A1, ASMItem::A2,
                                           ASMItem::A3, ASMItem::A4};
  const vector<ASMItem::RegType> aFRegs = {
      ASMItem::S0,  ASMItem::S1,  ASMItem::S2,  ASMItem::S3,
      ASMItem::S4,  ASMItem::S5,  ASMItem::S6,  ASMItem::S7,
      ASMItem::S8,  ASMItem::S9,  ASMItem::S10, ASMItem::S11,
      ASMItem::S12, ASMItem::S13, ASMItem::S14, ASMItem::S15};
  const vector<ASMItem::RegType> vIRegs = {
      ASMItem::V1, ASMItem::V2, ASMItem::V3, ASMItem::V4,
      ASMItem::V5, ASMItem::V6, ASMItem::V7, ASMItem::V8};
  const vector<ASMItem::RegType> vFRegs = {
      ASMItem::S16, ASMItem::S17, ASMItem::S18, ASMItem::S19,
      ASMItem::S20, ASMItem::S21, ASMItem::S22, ASMItem::S23,
      ASMItem::S24, ASMItem::S25, ASMItem::S26, ASMItem::S27,
      ASMItem::S28, ASMItem::S29, ASMItem::S30, ASMItem::S31};

  bool isProcessed;
  unsigned labelId;
  vector<Symbol *> consts;
  vector<Symbol *> globalVars;
  unordered_map<Symbol *, vector<Symbol *>> localVars;
  vector<pair<Symbol *, vector<IR *>>> funcIRs;
  unordered_map<IR *, unsigned> irLabels;
  vector<pair<Symbol *, vector<ASM *>>> funcASMs;
  unsigned usedIRegs;
  unsigned usedFRegs;
  unsigned spillRegs;
  unsigned savedRegs;
  unsigned frameOffset;
  unordered_map<unsigned, ASMItem::RegType> ftemp2Reg;
  unordered_map<unsigned, ASMItem::RegType> itemp2Reg;
  unordered_map<unsigned, int> temp2SpillReg;
  unordered_map<unsigned, int> spillOffsets;
  unordered_map<Symbol *, int> offsets;
  int startLineno;
  int stopLineno;

  void allocReg(const vector<IR *> &);
  int calcCallArgSize(const vector<IR *> &);
  void loadImmToReg(vector<ASM *> &, ASMItem::RegType, float);
  void loadImmToReg(vector<ASM *> &, ASMItem::RegType, int);
  void loadOrStoreFromSP(vector<ASM *> &, bool, ASMItem::RegType, unsigned);
  void initFrame();
  bool isFloatReg(ASMItem::RegType);
  void makeFrame(vector<ASM *> &, const vector<IR *> &, Symbol *);
  void moveFromSP(vector<ASM *> &, ASMItem::RegType, int);
  void parse();
  void parseAlgo(vector<ASM *> &, ASM::ASMOpType, ASM::ASMOpType, IR *);
  void parseB(vector<ASM *> &, IR *);
  void parseCall(vector<ASM *> &, IR *);
  void parseCmp(vector<ASM *> &, IR *);
  void parseF2I(vector<ASM *> &, IR *);
  vector<ASM *> parseFunc(Symbol *, const vector<IR *> &);
  void parseI2F(vector<ASM *> &, IR *);
  void parseLNot(vector<ASM *> &, IR *);
  void parseMemsetZero(vector<ASM *> &, IR *);
  void parseMovToFtemp(vector<ASM *> &, IR *);
  void parseMovToItemp(vector<ASM *> &, IR *);
  void parseMovToSymbol(vector<ASM *> &, IR *);
  void parseNeg(vector<ASM *> &, IR *);
  void parseReturn(vector<ASM *> &, IR *, IR *);
  void popArgs(vector<ASM *> &);
  void preProcess();
  void saveUsedRegs(vector<ASM *> &);
  void removeUnusedLabels(vector<ASM *> &);
  void saveArgRegs(vector<ASM *> &, Symbol *);

public:
  vector<pair<Symbol *, vector<ASM *>>> getFuncASMs();

  ASMParser(pair<unsigned, unsigned>, vector<pair<Symbol *, vector<IR *>>> &,
            vector<Symbol *> &, vector<Symbol *> &,
            unordered_map<Symbol *, vector<Symbol *>> &);
  ~ASMParser();
};

#endif