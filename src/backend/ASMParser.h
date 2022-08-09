#ifndef __ASM_PARSER_H__
#define __ASM_PARSER_H__

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../frontend/IR.h"
#include "../frontend/Symbol.h"
#include "ASM.h"
#include "Reg.h"

class ASMParser {
private:
  const std::vector<Reg::Type> aIRegs = {Reg::A1, Reg::A2, Reg::A3, Reg::A4};
  const std::vector<Reg::Type> aFRegs = {
      Reg::S0,  Reg::S1,  Reg::S2,  Reg::S3, Reg::S4,  Reg::S5,
      Reg::S6,  Reg::S7,  Reg::S8,  Reg::S9, Reg::S10, Reg::S11,
      Reg::S12, Reg::S13, Reg::S14, Reg::S15};
  const std::vector<Reg::Type> vIRegs = {Reg::V1, Reg::V2, Reg::V3, Reg::V4,
                                         Reg::V5, Reg::V6, Reg::V7, Reg::V8};
  const std::vector<Reg::Type> vFRegs = {
      Reg::S16, Reg::S17, Reg::S18, Reg::S19, Reg::S20, Reg::S21,
      Reg::S22, Reg::S23, Reg::S24, Reg::S25, Reg::S26, Reg::S27,
      Reg::S28, Reg::S29, Reg::S30, Reg::S31};

  bool isProcessed;
  unsigned labelId;
  std::vector<Symbol *> consts;
  std::vector<Symbol *> globalVars;
  std::unordered_map<Symbol *, std::vector<Symbol *>> localVars;
  std::unordered_map<Symbol *, std::vector<IR *>> funcIRs;
  std::unordered_map<IR *, unsigned> irLabels;
  std::unordered_map<Symbol *, std::vector<ASM *>> funcASMs;
  std::vector<unsigned> usedRegNum;
  unsigned savedRegs;
  unsigned frameOffset;
  std::unordered_map<unsigned, Reg::Type> ftemp2Reg;
  std::unordered_map<unsigned, Reg::Type> itemp2Reg;
  std::unordered_map<unsigned, unsigned> temp2SpillReg;
  std::unordered_map<unsigned, int> spillOffsets;
  std::unordered_map<Symbol *, int> offsets;
  unsigned startLineno;
  unsigned stopLineno;

  int calcCallArgSize(const std::vector<IR *> &);
  bool canBeLoadInSingleInstruction(unsigned);
  unsigned float2Unsigned(float);
  void loadFromSP(std::vector<ASM *> &, Reg::Type, unsigned);
  void loadImmToReg(std::vector<ASM *> &, Reg::Type, float);
  void loadImmToReg(std::vector<ASM *> &, Reg::Type, unsigned);
  void initFrame();
  bool isByteShiftImm(unsigned);
  bool isFloatImm(float);
  bool isFloatReg(Reg::Type);
  void makeFrame(std::vector<ASM *> &, std::vector<IR *> &, Symbol *);
  void moveFromSP(std::vector<ASM *> &, Reg::Type, int);
  void mulRegValue(std::vector<ASM *> &, Reg::Type, Reg::Type, int);
  void parse();
  void parseAdd(std::vector<ASM *> &, IR *);
  void parseAddFtempFloat(std::vector<ASM *> &, IR *);
  void parseAddFtempFtemp(std::vector<ASM *> &, IR *);
  void parseAddItempInt(std::vector<ASM *> &, IR *);
  void parseAddItempItemp(std::vector<ASM *> &, IR *);
  void parseB(std::vector<ASM *> &, IR *);
  void parseCall(std::vector<ASM *> &, IR *);
  void parseCmp(std::vector<ASM *> &, IR *);
  void parseDiv(std::vector<ASM *> &, IR *);
  void parseDivFloatFtemp(std::vector<ASM *> &, IR *);
  void parseDivFtempFloat(std::vector<ASM *> &, IR *);
  void parseDivFtempFtemp(std::vector<ASM *> &, IR *);
  void parseDivIntItemp(std::vector<ASM *> &, IR *);
  void parseDivItempInt(std::vector<ASM *> &, IR *);
  void parseDivItempItemp(std::vector<ASM *> &, IR *);
  void parseF2I(std::vector<ASM *> &, IR *);
  std::vector<ASM *> parseFunc(Symbol *, std::vector<IR *> &);
  void parseI2F(std::vector<ASM *> &, IR *);
  void parseLCmp(std::vector<ASM *> &, IR *);
  void parseLCmpFtempFloat(std::vector<ASM *> &, IR *);
  void parseLCmpFtempFtemp(std::vector<ASM *> &, IR *);
  void parseLCmpItempInt(std::vector<ASM *> &, IR *);
  void parseLCmpItempItemp(std::vector<ASM *> &, IR *);
  void parseLNot(std::vector<ASM *> &, IR *);
  void parseMod(std::vector<ASM *> &, IR *);
  void parseModIntItemp(std::vector<ASM *> &, IR *);
  void parseModItempInt(std::vector<ASM *> &, IR *);
  void parseModItempItemp(std::vector<ASM *> &, IR *);
  void parseMul(std::vector<ASM *> &, IR *);
  void parseMulFloatFtemp(std::vector<ASM *> &, IR *);
  void parseMulFtempFloat(std::vector<ASM *> &, IR *);
  void parseMulFtempFtemp(std::vector<ASM *> &, IR *);
  void parseMulItempInt(std::vector<ASM *> &, IR *);
  void parseMulItempItemp(std::vector<ASM *> &, IR *);
  void parseMov(std::vector<ASM *> &, IR *);
  void parseMovFtempFloat(std::vector<ASM *> &, IR *);
  void parseMovFtempFtemp(std::vector<ASM *> &, IR *);
  void parseMovFtempReturn(std::vector<ASM *> &, IR *);
  void parseMovFtempSymbol(std::vector<ASM *> &, IR *);
  void parseMovItempInt(std::vector<ASM *> &, IR *);
  void parseMovItempItemp(std::vector<ASM *> &, IR *);
  void parseMovItempReturn(std::vector<ASM *> &, IR *);
  void parseMovItempSymbol(std::vector<ASM *> &, IR *);
  void parseMovReturnFloat(std::vector<ASM *> &, IR *);
  void parseMovReturnFtemp(std::vector<ASM *> &, IR *);
  void parseMovReturnInt(std::vector<ASM *> &, IR *);
  void parseMovReturnItemp(std::vector<ASM *> &, IR *);
  void parseMovReturnSymbol(std::vector<ASM *> &, IR *);
  void parseMovSymbolFtemp(std::vector<ASM *> &, IR *);
  void parseMovSymbolImm(std::vector<ASM *> &, IR *);
  void parseMovSymbolItemp(std::vector<ASM *> &, IR *);
  void parseMovSymbolReturn(std::vector<ASM *> &, IR *);
  void parseNeg(std::vector<ASM *> &, IR *);
  void parseSub(std::vector<ASM *> &, IR *);
  void parseSubFloatFtemp(std::vector<ASM *> &, IR *);
  void parseSubFtempFloat(std::vector<ASM *> &, IR *);
  void parseSubFtempFtemp(std::vector<ASM *> &, IR *);
  void parseSubIntItemp(std::vector<ASM *> &, IR *);
  void parseSubItempInt(std::vector<ASM *> &, IR *);
  void parseSubItempItemp(std::vector<ASM *> &, IR *);
  void popArgs(std::vector<ASM *> &);
  void preProcess();
  void saveUsedRegs(std::vector<ASM *> &);
  void saveArgRegs(std::vector<ASM *> &, Symbol *);
  void storeFromSP(std::vector<ASM *> &, Reg::Type, unsigned);
  void switchLCmpLogic(IR *);

public:
  std::unordered_map<Symbol *, std::vector<ASM *>> getFuncASMs();

  ASMParser(std::pair<unsigned, unsigned>,
            std::unordered_map<Symbol *, std::vector<IR *>> &,
            std::vector<Symbol *> &, std::vector<Symbol *> &,
            std::unordered_map<Symbol *, std::vector<Symbol *>> &);
  ~ASMParser();
};

#endif