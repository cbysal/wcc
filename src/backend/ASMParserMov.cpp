#include "ASMParser.h"
#include "HardCoding.h"

using namespace std;

void ASMParser::addTempsToReg(vector<ASM *> &asms,
                              vector<pair<unsigned, unsigned>> &temps,
                              Reg::Type target) {
  for (pair<unsigned, unsigned> temp : temps) {
    bool flag = itemp2Reg.find(temp.first) == itemp2Reg.end();
    if (flag)
      loadFromSP(asms, Reg::A3, spillOffsets[temp.first]);
    if (num2powerMap.find(temp.second) == num2powerMap.end()) {
      loadImmToReg(asms, Reg::A4, temp.second);
      asms.push_back(new ASM(
          ASM::MLA, {new ASMItem(target),
                     new ASMItem(flag ? Reg::A3 : itemp2Reg[temp.first]),
                     new ASMItem(Reg::A4), new ASMItem(target)}));
    } else
      asms.push_back(new ASM(
          ASM::ADD, {new ASMItem(target), new ASMItem(target),
                     new ASMItem(flag ? Reg::A3 : itemp2Reg[temp.first]),
                     new ASMItem(ASMItem::LSL, num2powerMap[temp.second])}));
  }
}

pair<vector<pair<unsigned, unsigned>>, unsigned>
ASMParser::makeTempsOffset(IR *ir, Symbol *symbol) {
  if (symbol->dimensions.empty())
    return {{}, 0};
  vector<unsigned> sizes(symbol->dimensions.size());
  sizes.back() = 4;
  for (int i = symbol->dimensions.size() - 1; i > 0; i--)
    sizes[i - 1] = sizes[i] * symbol->dimensions[i];
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
  return {temps, offset};
}

void ASMParser::parseMov(vector<ASM *> &asms, IR *ir) {
  if (ir->items[0]->type == IRItem::FTEMP &&
      ir->items[1]->type == IRItem::FLOAT)
    parseMovFtempFloat(asms, ir);
  else if (ir->items[0]->type == IRItem::FTEMP &&
           ir->items[1]->type == IRItem::FTEMP)
    parseMovFtempFtemp(asms, ir);
  else if (ir->items[0]->type == IRItem::FTEMP &&
           ir->items[1]->type == IRItem::RETURN)
    parseMovFtempReturn(asms, ir);
  else if (ir->items[0]->type == IRItem::FTEMP &&
           ir->items[1]->type == IRItem::SYMBOL)
    parseMovFtempSymbol(asms, ir);
  else if (ir->items[0]->type == IRItem::ITEMP &&
           ir->items[1]->type == IRItem::INT)
    parseMovItempInt(asms, ir);
  else if (ir->items[0]->type == IRItem::ITEMP &&
           ir->items[1]->type == IRItem::ITEMP)
    parseMovItempItemp(asms, ir);
  else if (ir->items[0]->type == IRItem::ITEMP &&
           ir->items[1]->type == IRItem::RETURN)
    parseMovItempReturn(asms, ir);
  else if (ir->items[0]->type == IRItem::ITEMP &&
           ir->items[1]->type == IRItem::SYMBOL)
    parseMovItempSymbol(asms, ir);
  else if (ir->items[0]->type == IRItem::RETURN &&
           ir->items[1]->type == IRItem::FLOAT)
    parseMovReturnFloat(asms, ir);
  else if (ir->items[0]->type == IRItem::RETURN &&
           ir->items[1]->type == IRItem::FTEMP)
    parseMovReturnFtemp(asms, ir);
  else if (ir->items[0]->type == IRItem::RETURN &&
           ir->items[1]->type == IRItem::INT)
    parseMovReturnInt(asms, ir);
  else if (ir->items[0]->type == IRItem::RETURN &&
           ir->items[1]->type == IRItem::ITEMP)
    parseMovReturnItemp(asms, ir);
  else if (ir->items[0]->type == IRItem::RETURN &&
           ir->items[1]->type == IRItem::SYMBOL)
    parseMovReturnSymbol(asms, ir);
  else if (ir->items[0]->type == IRItem::SYMBOL &&
           ir->items[1]->type == IRItem::FTEMP)
    parseMovSymbolFtemp(asms, ir);
  else if (ir->items[0]->type == IRItem::SYMBOL &&
           (ir->items[1]->type == IRItem::FLOAT ||
            ir->items[1]->type == IRItem::INT))
    parseMovSymbolImm(asms, ir);
  else if (ir->items[0]->type == IRItem::SYMBOL &&
           ir->items[1]->type == IRItem::ITEMP)
    parseMovSymbolItemp(asms, ir);
  else if (ir->items[0]->type == IRItem::SYMBOL &&
           ir->items[1]->type == IRItem::RETURN)
    parseMovSymbolReturn(asms, ir);
}

void ASMParser::parseMovFtempFloat(vector<ASM *> &asms, IR *ir) {
  if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end()) {
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
  } else
    loadImmToReg(asms, ftemp2Reg[ir->items[0]->iVal], ir->items[1]->fVal);
}

void ASMParser::parseMovFtempFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (flag1 && flag2) {
    loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (flag1 && !flag2)
    storeFromSP(asms, ftemp2Reg[ir->items[1]->iVal],
                spillOffsets[ir->items[0]->iVal]);
  else if (!flag1 && flag2)
    loadFromSP(asms, ftemp2Reg[ir->items[0]->iVal],
               spillOffsets[ir->items[1]->iVal]);
  else if (!flag1 && !flag2)
    asms.push_back(
        new ASM(ASM::VMOV, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                            new ASMItem(ftemp2Reg[ir->items[1]->iVal])}));
}

void ASMParser::parseMovFtempReturn(vector<ASM *> &asms, IR *ir) {
  if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
  else
    asms.push_back(
        new ASM(ASM::VMOV, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                            new ASMItem(Reg::S0)}));
}

void ASMParser::parseMovFtempSymbol(vector<ASM *> &asms, IR *ir) {
  pair<vector<pair<unsigned, unsigned>>, unsigned> tempsOffset =
      makeTempsOffset(ir, ir->items[1]->symbol);
  vector<pair<unsigned, unsigned>> &temps = tempsOffset.first;
  unsigned offset = tempsOffset.second;
  if (temps.empty() && offset)
    parseMovFtempSymbolOffset(asms, ir, offset);
  else if (temps.empty() && !offset)
    parseMovFtempSymbolNone(asms, ir);
  else if (!temps.empty() && offset)
    parseMovFtempSymbolTempsOffset(asms, ir, temps, offset);
  else if (!temps.empty() && !offset)
    parseMovFtempSymbolTemps(asms, ir, temps);
}

void ASMParser::parseMovFtempSymbolNone(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end();
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A1),
                    new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A1),
                    new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
    if (flag1) {
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                              new ASMItem(Reg::A1)}));
    break;
  case Symbol::LOCAL_VAR:
    if (flag1) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else
      loadFromSP(asms, ftemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
    break;
  case Symbol::PARAM:
    if (flag1 && ir->items[1]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !ir->items[1]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && ir->items[1]->symbol->dimensions.empty())
      loadFromSP(asms, ftemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
    else if (!flag1 && !ir->items[1]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                              new ASMItem(Reg::A1)}));
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovFtempSymbolOffset(vector<ASM *> &asms, IR *ir,
                                          unsigned offset) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end();
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A1),
                    new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A1),
                    new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
    if (flag1) {
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else
      loadFromReg(asms, ftemp2Reg[ir->items[0]->iVal], Reg::A1, offset);
    break;
  case Symbol::LOCAL_VAR:
    if (flag1) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol] + offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else
      loadFromSP(asms, ftemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol] + offset);
    break;
  case Symbol::PARAM:
    loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
    if (flag1) {
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else
      loadFromReg(asms, ftemp2Reg[ir->items[0]->iVal], Reg::A1, offset);
    break;
  default:
    break;
  }
}

void ASMParser::parseMovFtempSymbolTemps(
    vector<ASM *> &asms, IR *ir, vector<pair<unsigned, unsigned>> &temps) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end();
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A1),
                    new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A1),
                    new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
    addTempsToReg(asms, temps, Reg::A1);
    if (flag1) {
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                              new ASMItem(Reg::A1)}));
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
    addTempsToReg(asms, temps, Reg::A1);
    if (flag1) {
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                              new ASMItem(Reg::A1)}));
    break;
  case Symbol::PARAM:
    loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
    addTempsToReg(asms, temps, Reg::A1);
    if (flag1) {
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                              new ASMItem(Reg::A1)}));
    break;
  default:
    break;
  }
}

void ASMParser::parseMovFtempSymbolTempsOffset(
    vector<ASM *> &asms, IR *ir, vector<pair<unsigned, unsigned>> &temps,
    unsigned offset) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end();
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A1),
                    new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A1),
                    new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
    addTempsToReg(asms, temps, Reg::A1);
    if (flag1) {
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else
      loadFromReg(asms, ftemp2Reg[ir->items[0]->iVal], Reg::A1, offset);
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
    addTempsToReg(asms, temps, Reg::A1);
    if (flag1) {
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else
      loadFromReg(asms, ftemp2Reg[ir->items[0]->iVal], Reg::A1, offset);
    break;
  case Symbol::PARAM:
    loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
    addTempsToReg(asms, temps, Reg::A1);
    if (flag1) {
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else
      loadFromReg(asms, ftemp2Reg[ir->items[0]->iVal], Reg::A1, offset);
    break;
  default:
    break;
  }
}

void ASMParser::parseMovItempInt(vector<ASM *> &asms, IR *ir) {
  if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end()) {
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
  } else
    loadImmToReg(asms, itemp2Reg[ir->items[0]->iVal],
                 (unsigned)ir->items[1]->iVal);
}

void ASMParser::parseMovItempItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag1 && flag2) {
    loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (flag1 && !flag2)
    storeFromSP(asms, itemp2Reg[ir->items[1]->iVal],
                spillOffsets[ir->items[0]->iVal]);
  else if (!flag1 && flag2)
    loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
               spillOffsets[ir->items[1]->iVal]);
  else if (!flag1 && !flag2)
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                           new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
}

void ASMParser::parseMovItempReturn(vector<ASM *> &asms, IR *ir) {
  if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
  else
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                           new ASMItem(Reg::A1)}));
}

void ASMParser::parseMovItempSymbol(vector<ASM *> &asms, IR *ir) {
  pair<vector<pair<unsigned, unsigned>>, unsigned> tempsOffset =
      makeTempsOffset(ir, ir->items[1]->symbol);
  vector<pair<unsigned, unsigned>> &temps = tempsOffset.first;
  unsigned offset = tempsOffset.second;
  if (temps.empty() && offset)
    parseMovItempSymbolOffset(asms, ir, offset);
  else if (temps.empty() && !offset)
    parseMovItempSymbolNone(asms, ir);
  else if (!temps.empty() && offset)
    parseMovItempSymbolTempsOffset(asms, ir, temps, offset);
  else if (!temps.empty() && !offset)
    parseMovItempSymbolTemps(asms, ir, temps);
}

void ASMParser::parseMovItempSymbolNone(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end();
  bool isFull = ir->items[1]->symbol->dimensions.size() + 2 == ir->items.size();
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (flag1 && isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(itemp2Reg[ir->items[0]->iVal])}));
    } else if (!flag1 && !isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
    }
    break;
  case Symbol::LOCAL_VAR:
    if (flag1 && isFull) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull) {
      moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull)
      loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
    else if (!flag1 && !isFull)
      moveFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
    break;
  case Symbol::PARAM:
    if (flag1 && isFull && ir->items[1]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && isFull && !ir->items[1]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull && ir->items[1]->symbol->dimensions.empty()) {
      moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull && !ir->items[1]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull && ir->items[1]->symbol->dimensions.empty())
      loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
    else if (!flag1 && isFull && !ir->items[1]->symbol->dimensions.empty()) {
      loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(itemp2Reg[ir->items[0]->iVal])}));
    } else if (!flag1 && !isFull && ir->items[1]->symbol->dimensions.empty())
      moveFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
    else if (!flag1 && !isFull && !ir->items[1]->symbol->dimensions.empty())
      loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
    break;
  default:
    break;
  }
}

void ASMParser::parseMovItempSymbolOffset(vector<ASM *> &asms, IR *ir,
                                          unsigned offset) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end();
  bool isFull = ir->items[1]->symbol->dimensions.size() + 2 == ir->items.size();
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (flag1 && isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      loadFromReg(asms, itemp2Reg[ir->items[0]->iVal],
                  itemp2Reg[ir->items[0]->iVal], offset);
    } else if (!flag1 && !isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      moveFromReg(asms, itemp2Reg[ir->items[0]->iVal],
                  itemp2Reg[ir->items[0]->iVal], offset);
    }
    break;
  case Symbol::LOCAL_VAR:
    if (flag1 && isFull) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol] + offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull) {
      moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol] + offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull)
      loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol] + offset);
    else if (!flag1 && !isFull)
      moveFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol] + offset);
    break;
  case Symbol::PARAM:
    if (flag1 && isFull) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      moveFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull) {
      loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      loadFromReg(asms, itemp2Reg[ir->items[0]->iVal],
                  itemp2Reg[ir->items[0]->iVal], offset);
    } else if (!flag1 && !isFull) {
      loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      moveFromReg(asms, itemp2Reg[ir->items[0]->iVal],
                  itemp2Reg[ir->items[0]->iVal], offset);
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovItempSymbolTemps(
    vector<ASM *> &asms, IR *ir, vector<pair<unsigned, unsigned>> &temps) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end();
  bool isFull = ir->items[1]->symbol->dimensions.size() + 2 == ir->items.size();
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (flag1 && isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A1);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      addTempsToReg(asms, temps, itemp2Reg[ir->items[0]->iVal]);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(itemp2Reg[ir->items[0]->iVal])}));
    } else if (!flag1 && !isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      addTempsToReg(asms, temps, itemp2Reg[ir->items[0]->iVal]);
    }
    break;
  case Symbol::LOCAL_VAR:
    if (flag1 && isFull) {
      moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull) {
      moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull) {
      moveFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, itemp2Reg[ir->items[0]->iVal]);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(itemp2Reg[ir->items[0]->iVal])}));
    } else if (!flag1 && !isFull) {
      moveFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, itemp2Reg[ir->items[0]->iVal]);
    }
    break;
  case Symbol::PARAM:
    if (flag1 && isFull) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull) {
      loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, itemp2Reg[ir->items[0]->iVal]);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(itemp2Reg[ir->items[0]->iVal])}));
    } else if (!flag1 && !isFull) {
      loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, itemp2Reg[ir->items[0]->iVal]);
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovItempSymbolTempsOffset(
    vector<ASM *> &asms, IR *ir, vector<pair<unsigned, unsigned>> &temps,
    unsigned offset) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end();
  bool isFull = ir->items[1]->symbol->dimensions.size() + 2 == ir->items.size();
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (flag1 && isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A1);
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A1);
      moveFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      addTempsToReg(asms, temps, itemp2Reg[ir->items[0]->iVal]);
      loadFromReg(asms, itemp2Reg[ir->items[0]->iVal],
                  itemp2Reg[ir->items[0]->iVal], offset);
    } else if (!flag1 && !isFull) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      addTempsToReg(asms, temps, itemp2Reg[ir->items[0]->iVal]);
      moveFromReg(asms, itemp2Reg[ir->items[0]->iVal],
                  itemp2Reg[ir->items[0]->iVal], offset);
    }
    break;
  case Symbol::LOCAL_VAR:
    if (flag1 && isFull) {
      moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull) {
      moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      moveFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull) {
      moveFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, itemp2Reg[ir->items[0]->iVal]);
      loadFromReg(asms, itemp2Reg[ir->items[0]->iVal],
                  itemp2Reg[ir->items[0]->iVal], offset);
    } else if (!flag1 && !isFull) {
      moveFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, itemp2Reg[ir->items[0]->iVal]);
      moveFromReg(asms, itemp2Reg[ir->items[0]->iVal],
                  itemp2Reg[ir->items[0]->iVal], offset);
    }
    break;
  case Symbol::PARAM:
    if (flag1 && isFull) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      moveFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull) {
      loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, itemp2Reg[ir->items[0]->iVal]);
      loadFromReg(asms, itemp2Reg[ir->items[0]->iVal],
                  itemp2Reg[ir->items[0]->iVal], offset);
    } else if (!flag1 && !isFull) {
      loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, itemp2Reg[ir->items[0]->iVal]);
      moveFromReg(asms, Reg::A1, Reg::A1, offset);
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovReturnFloat(vector<ASM *> &asms, IR *ir) {
  loadImmToReg(asms, Reg::S0, ir->items[1]->fVal);
}

void ASMParser::parseMovReturnFtemp(vector<ASM *> &asms, IR *ir) {
  if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end())
    loadFromSP(asms, Reg::S0, spillOffsets[ir->items[1]->iVal]);
  else
    asms.push_back(
        new ASM(ASM::VMOV, {new ASMItem(Reg::S0),
                            new ASMItem(ftemp2Reg[ir->items[1]->iVal])}));
}

void ASMParser::parseMovReturnInt(vector<ASM *> &asms, IR *ir) {
  loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
}

void ASMParser::parseMovReturnItemp(vector<ASM *> &asms, IR *ir) {
  if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end())
    loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
  else
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(Reg::A1),
                           new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
}

void ASMParser::parseMovReturnSymbol(vector<ASM *> &asms, IR *ir) {
  pair<vector<pair<unsigned, unsigned>>, unsigned> tempsOffset =
      makeTempsOffset(ir, ir->items[1]->symbol);
  vector<pair<unsigned, unsigned>> &temps = tempsOffset.first;
  unsigned offset = tempsOffset.second;
  if (temps.empty() && offset)
    parseMovReturnSymbolOffset(asms, ir, offset);
  else if (temps.empty() && !offset)
    parseMovReturnSymbolNone(asms, ir);
  else if (!temps.empty() && offset)
    parseMovReturnSymbolTempsOffset(asms, ir, temps, offset);
  else if (!temps.empty() && !offset)
    parseMovReturnSymbolTemps(asms, ir, temps);
}

void ASMParser::parseMovReturnSymbolNone(vector<ASM *> &asms, IR *ir) {
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A1),
                    new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A1),
                    new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
    if (ir->items[1]->symbol->dataType == Symbol::FLOAT) {
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(Reg::S0), new ASMItem(Reg::A1)}));
    } else
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
    break;
  case Symbol::LOCAL_VAR:
    if (ir->items[1]->symbol->dataType == Symbol::FLOAT)
      loadFromSP(asms, Reg::S0, offsets[ir->items[1]->symbol]);
    else
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
    break;
  case Symbol::PARAM:
    if (ir->items[1]->symbol->dataType == Symbol::FLOAT &&
        ir->items[1]->symbol->dimensions.empty())
      loadFromSP(asms, Reg::S0, offsets[ir->items[1]->symbol]);
    else if (ir->items[1]->symbol->dataType == Symbol::FLOAT &&
             !ir->items[1]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(Reg::S0), new ASMItem(Reg::A1)}));
    } else if (ir->items[1]->symbol->dataType != Symbol::FLOAT &&
               ir->items[1]->symbol->dimensions.empty())
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
    else if (ir->items[1]->symbol->dataType != Symbol::FLOAT &&
             !ir->items[1]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovReturnSymbolOffset(vector<ASM *> &asms, IR *ir,
                                           unsigned offset) {
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A1),
                    new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A1),
                    new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
    if (ir->items[1]->symbol->dataType == Symbol::FLOAT)
      loadFromReg(asms, Reg::S0, Reg::A1, offset);
    else
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
    break;
  case Symbol::LOCAL_VAR:
    if (ir->items[1]->symbol->dataType == Symbol::FLOAT)
      loadFromSP(asms, Reg::S0, offsets[ir->items[1]->symbol] + offset);
    else
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol] + offset);
    break;
  case Symbol::PARAM:
    loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
    if (ir->items[1]->symbol->dataType == Symbol::FLOAT)
      loadFromReg(asms, Reg::S0, Reg::A1, offset);
    else
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
    break;
  default:
    break;
  }
}

void ASMParser::parseMovReturnSymbolTemps(
    vector<ASM *> &asms, IR *ir, vector<pair<unsigned, unsigned>> &temps) {
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A1),
                    new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A1),
                    new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
    addTempsToReg(asms, temps, Reg::A1);
    if (ir->items[1]->symbol->dataType == Symbol::FLOAT)
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(Reg::S0), new ASMItem(Reg::A1)}));
    else
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
    addTempsToReg(asms, temps, Reg::A1);
    if (ir->items[1]->symbol->dataType == Symbol::FLOAT)
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(Reg::S0), new ASMItem(Reg::A1)}));
    else
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
    break;
  case Symbol::PARAM:
    loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
    addTempsToReg(asms, temps, Reg::A1);
    if (ir->items[1]->symbol->dataType == Symbol::FLOAT)
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(Reg::S0), new ASMItem(Reg::A1)}));
    else
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
    break;
  default:
    break;
  }
}

void ASMParser::parseMovReturnSymbolTempsOffset(
    vector<ASM *> &asms, IR *ir, vector<pair<unsigned, unsigned>> &temps,
    unsigned offset) {
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A1),
                    new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A1),
                    new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
    addTempsToReg(asms, temps, Reg::A1);
    if (ir->items[1]->symbol->dataType == Symbol::FLOAT)
      loadFromReg(asms, Reg::S0, Reg::A1, offset);
    else
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
    addTempsToReg(asms, temps, Reg::A1);
    if (ir->items[1]->symbol->dataType == Symbol::FLOAT)
      loadFromReg(asms, Reg::S0, Reg::A1, offset);
    else
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
    break;
  case Symbol::PARAM:
    loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
    addTempsToReg(asms, temps, Reg::A1);
    if (ir->items[1]->symbol->dataType == Symbol::FLOAT)
      loadFromReg(asms, Reg::S0, Reg::A1, offset);
    else
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolFtemp(vector<ASM *> &asms, IR *ir) {
  pair<vector<pair<unsigned, unsigned>>, unsigned> tempsOffset =
      makeTempsOffset(ir, ir->items[0]->symbol);
  vector<pair<unsigned, unsigned>> &temps = tempsOffset.first;
  unsigned offset = tempsOffset.second;
  if (temps.empty() && offset)
    parseMovSymbolFtempOffset(asms, ir, offset);
  else if (temps.empty() && !offset)
    parseMovSymbolFtempNone(asms, ir);
  else if (!temps.empty() && offset)
    parseMovSymbolFtempTempsOffset(asms, ir, temps, offset);
  else if (!temps.empty() && !offset)
    parseMovSymbolFtempTemps(asms, ir, temps);
}

void ASMParser::parseMovSymbolFtempNone(vector<ASM *> &asms, IR *ir) {
  bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A2),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A2),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    } else {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      asms.push_back(
          new ASM(ASM::VSTR, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                              new ASMItem(Reg::A1)}));
    }
    break;
  case Symbol::LOCAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      storeFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
    } else
      storeFromSP(asms, ftemp2Reg[ir->items[1]->iVal],
                  offsets[ir->items[0]->symbol]);
    break;
  case Symbol::PARAM:
    if (flag2 && ir->items[0]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      storeFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
    } else if (flag2 && !ir->items[0]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    } else if (!flag2 && ir->items[0]->symbol->dimensions.empty())
      storeFromSP(asms, ftemp2Reg[ir->items[1]->iVal],
                  offsets[ir->items[0]->symbol]);
    else if (!flag2 && !ir->items[0]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      asms.push_back(
          new ASM(ASM::VSTR, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                              new ASMItem(Reg::A1)}));
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolFtempOffset(vector<ASM *> &asms, IR *ir,
                                          unsigned offset) {
  bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A2),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A2),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      storeFromReg(asms, Reg::A1, Reg::A2, offset);
    } else {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      storeFromReg(asms, ftemp2Reg[ir->items[1]->iVal], Reg::A1, offset);
    }
    break;
  case Symbol::LOCAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      storeFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol] + offset);
    } else
      storeFromSP(asms, ftemp2Reg[ir->items[1]->iVal],
                  offsets[ir->items[0]->symbol] + offset);
    break;
  case Symbol::PARAM:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      storeFromReg(asms, Reg::A1, Reg::A2, offset);
    } else {
      loadFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      storeFromReg(asms, ftemp2Reg[ir->items[1]->iVal], Reg::A1, offset);
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolFtempTemps(
    vector<ASM *> &asms, IR *ir, vector<pair<unsigned, unsigned>> &temps) {
  bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A2),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A2),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A2);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    } else {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::VSTR, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                              new ASMItem(Reg::A1)}));
    }
    break;
  case Symbol::LOCAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A2);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    } else {
      moveFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::VSTR, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                              new ASMItem(Reg::A1)}));
    }
    break;
  case Symbol::PARAM:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A2);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    } else {
      loadFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::VSTR, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                              new ASMItem(Reg::A1)}));
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolFtempTempsOffset(
    vector<ASM *> &asms, IR *ir, vector<pair<unsigned, unsigned>> &temps,
    unsigned offset) {
  bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A2),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A2),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A2);
      storeFromReg(asms, Reg::A1, Reg::A2, offset);
    } else {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A1);
      storeFromReg(asms, ftemp2Reg[ir->items[1]->iVal], Reg::A1, offset);
    }
    break;
  case Symbol::LOCAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A2);
      storeFromReg(asms, Reg::A1, Reg::A2, offset);
    } else {
      moveFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      storeFromReg(asms, ftemp2Reg[ir->items[1]->iVal], Reg::A1, offset);
    }
    break;
  case Symbol::PARAM:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A2);
      storeFromReg(asms, Reg::A1, Reg::A2, offset);
    } else {
      loadFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      storeFromReg(asms, ftemp2Reg[ir->items[1]->iVal], Reg::A1, offset);
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolImm(vector<ASM *> &asms, IR *ir) {
  pair<vector<pair<unsigned, unsigned>>, unsigned> tempsOffset =
      makeTempsOffset(ir, ir->items[0]->symbol);
  vector<pair<unsigned, unsigned>> &temps = tempsOffset.first;
  unsigned offset = tempsOffset.second;
  if (temps.empty() && offset)
    parseMovSymbolImmOffset(asms, ir, offset);
  else if (temps.empty() && !offset)
    parseMovSymbolImmNone(asms, ir);
  else if (!temps.empty() && offset)
    parseMovSymbolImmTempsOffset(asms, ir, temps, offset);
  else if (!temps.empty() && !offset)
    parseMovSymbolImmTemps(asms, ir, temps);
}

void ASMParser::parseMovSymbolImmNone(vector<ASM *> &asms, IR *ir) {
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    break;
  case Symbol::LOCAL_VAR:
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    storeFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
    break;
  case Symbol::PARAM:
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    if (ir->items[0]->symbol->dimensions.empty())
      storeFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
    else {
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolImmOffset(vector<ASM *> &asms, IR *ir,
                                        unsigned offset) {
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
    storeFromReg(asms, Reg::A1, Reg::A2, offset);
    break;
  case Symbol::LOCAL_VAR:
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    storeFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol] + offset);
    break;
  case Symbol::PARAM:
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    storeFromReg(asms, Reg::A1, Reg::A2, offset);
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolImmTemps(
    vector<ASM *> &asms, IR *ir, vector<pair<unsigned, unsigned>> &temps) {
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
    addTempsToReg(asms, temps, Reg::A2);
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    break;
  case Symbol::LOCAL_VAR:
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    addTempsToReg(asms, temps, Reg::A2);
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    break;
  case Symbol::PARAM:
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    addTempsToReg(asms, temps, Reg::A2);
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolImmTempsOffset(
    vector<ASM *> &asms, IR *ir, vector<pair<unsigned, unsigned>> &temps,
    unsigned offset) {
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
    addTempsToReg(asms, temps, Reg::A2);
    storeFromReg(asms, Reg::A1, Reg::A2, offset);
    break;
  case Symbol::LOCAL_VAR:
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    addTempsToReg(asms, temps, Reg::A2);
    storeFromReg(asms, Reg::A1, Reg::A2, offset);
    break;
  case Symbol::PARAM:
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    addTempsToReg(asms, temps, Reg::A2);
    storeFromReg(asms, Reg::A1, Reg::A2, offset);
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolItemp(vector<ASM *> &asms, IR *ir) {
  pair<vector<pair<unsigned, unsigned>>, unsigned> tempsOffset =
      makeTempsOffset(ir, ir->items[0]->symbol);
  vector<pair<unsigned, unsigned>> &temps = tempsOffset.first;
  unsigned offset = tempsOffset.second;
  if (temps.empty() && offset)
    parseMovSymbolItempOffset(asms, ir, offset);
  else if (temps.empty() && !offset)
    parseMovSymbolItempNone(asms, ir);
  else if (!temps.empty() && offset)
    parseMovSymbolItempTempsOffset(asms, ir, temps, offset);
  else if (!temps.empty() && !offset)
    parseMovSymbolItempTemps(asms, ir, temps);
}

void ASMParser::parseMovSymbolItempNone(vector<ASM *> &asms, IR *ir) {
  bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A2),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A2),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    } else {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                             new ASMItem(Reg::A1)}));
    }
    break;
  case Symbol::LOCAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      storeFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
    } else
      storeFromSP(asms, itemp2Reg[ir->items[1]->iVal],
                  offsets[ir->items[0]->symbol]);
    break;
  case Symbol::PARAM:
    if (flag2 && ir->items[0]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      storeFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
    } else if (flag2 && !ir->items[0]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    } else if (!flag2 && ir->items[0]->symbol->dimensions.empty())
      storeFromSP(asms, itemp2Reg[ir->items[1]->iVal],
                  offsets[ir->items[0]->symbol]);
    else if (!flag2 && !ir->items[0]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                             new ASMItem(Reg::A1)}));
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolItempOffset(vector<ASM *> &asms, IR *ir,
                                          unsigned offset) {
  bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A2),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A2),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      storeFromReg(asms, Reg::A1, Reg::A2, offset);
    } else {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      storeFromReg(asms, itemp2Reg[ir->items[1]->iVal], Reg::A1, offset);
    }
    break;
  case Symbol::LOCAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      storeFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol] + offset);
    } else
      storeFromSP(asms, itemp2Reg[ir->items[1]->iVal],
                  offsets[ir->items[0]->symbol] + offset);
    break;
  case Symbol::PARAM:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      storeFromReg(asms, Reg::A1, Reg::A2, offset);
    } else {
      loadFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      storeFromReg(asms, itemp2Reg[ir->items[1]->iVal], Reg::A1, offset);
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolItempTemps(
    vector<ASM *> &asms, IR *ir, vector<pair<unsigned, unsigned>> &temps) {
  bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A2),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A2),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A2);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    } else {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                             new ASMItem(Reg::A1)}));
    }
    break;
  case Symbol::LOCAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A2);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    } else {
      moveFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                             new ASMItem(Reg::A1)}));
    }
    break;
  case Symbol::PARAM:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A2);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    } else {
      loadFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                             new ASMItem(Reg::A1)}));
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolItempTempsOffset(
    vector<ASM *> &asms, IR *ir, vector<pair<unsigned, unsigned>> &temps,
    unsigned offset) {
  bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A2),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A2),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A2);
      storeFromReg(asms, Reg::A1, Reg::A2, offset);
    } else {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A1);
      storeFromReg(asms, itemp2Reg[ir->items[1]->iVal], Reg::A1, offset);
    }
    break;
  case Symbol::LOCAL_VAR:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A2);
      storeFromReg(asms, Reg::A1, Reg::A2, offset);
    } else {
      moveFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      storeFromReg(asms, itemp2Reg[ir->items[1]->iVal], Reg::A1, offset);
    }
    break;
  case Symbol::PARAM:
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A2);
      storeFromReg(asms, Reg::A1, Reg::A2, offset);
    } else {
      loadFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      storeFromReg(asms, itemp2Reg[ir->items[1]->iVal], Reg::A1, offset);
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolReturn(vector<ASM *> &asms, IR *ir) {
  pair<vector<pair<unsigned, unsigned>>, unsigned> tempsOffset =
      makeTempsOffset(ir, ir->items[0]->symbol);
  vector<pair<unsigned, unsigned>> &temps = tempsOffset.first;
  unsigned offset = tempsOffset.second;
  if (temps.empty() && offset)
    parseMovSymbolReturnOffset(asms, ir, offset);
  else if (temps.empty() && !offset)
    parseMovSymbolReturnNone(asms, ir);
  else if (!temps.empty() && offset)
    parseMovSymbolReturnTempsOffset(asms, ir, temps, offset);
  else if (!temps.empty() && !offset)
    parseMovSymbolReturnTemps(asms, ir, temps);
}

void ASMParser::parseMovSymbolReturnNone(vector<ASM *> &asms, IR *ir) {
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (ir->items[0]->symbol->dataType == Symbol::FLOAT) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      asms.push_back(
          new ASM(ASM::VSTR, {new ASMItem(Reg::S0), new ASMItem(Reg::A1)}));
    } else {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A2),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A2),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    }
    break;
  case Symbol::LOCAL_VAR:
    if (ir->items[0]->symbol->dataType == Symbol::FLOAT) {
      moveFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      asms.push_back(
          new ASM(ASM::VSTR, {new ASMItem(Reg::S0), new ASMItem(Reg::A1)}));
    } else {
      moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    }
    break;
  case Symbol::PARAM:
    if (ir->items[0]->symbol->dataType == Symbol::FLOAT &&
        ir->items[0]->symbol->dimensions.empty())
      storeFromSP(asms, Reg::S0, offsets[ir->items[0]->symbol]);
    else if (ir->items[0]->symbol->dataType == Symbol::FLOAT &&
             !ir->items[0]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      asms.push_back(
          new ASM(ASM::VSTR, {new ASMItem(Reg::S0), new ASMItem(Reg::A1)}));
    } else if (ir->items[0]->symbol->dataType != Symbol::FLOAT &&
               ir->items[0]->symbol->dimensions.empty())
      storeFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
    else if (ir->items[0]->symbol->dataType != Symbol::FLOAT &&
             !ir->items[0]->symbol->dimensions.empty()) {
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolReturnOffset(vector<ASM *> &asms, IR *ir,
                                           unsigned offset) {
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (ir->items[0]->symbol->dataType == Symbol::FLOAT) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      storeFromReg(asms, Reg::S0, Reg::A1, offset);
    } else {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A2),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A2),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      storeFromReg(asms, Reg::A1, Reg::A2, offset);
    }
    break;
  case Symbol::LOCAL_VAR:
    if (ir->items[0]->symbol->dataType == Symbol::FLOAT)
      storeFromSP(asms, Reg::S0, offsets[ir->items[0]->symbol] + offset);
    else
      storeFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol] + offset);
    break;
  case Symbol::PARAM:
    if (ir->items[0]->symbol->dataType == Symbol::FLOAT) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      storeFromReg(asms, Reg::S0, Reg::A1, offset);
    } else {
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      storeFromReg(asms, Reg::A1, Reg::A2, offset);
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolReturnTemps(
    vector<ASM *> &asms, IR *ir, vector<pair<unsigned, unsigned>> &temps) {
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (ir->items[0]->symbol->dataType == Symbol::FLOAT) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::VSTR, {new ASMItem(Reg::S0), new ASMItem(Reg::A1)}));
    } else {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A2),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A2),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A2);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    }
    break;
  case Symbol::LOCAL_VAR:
    if (ir->items[0]->symbol->dataType == Symbol::FLOAT) {
      moveFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::VSTR, {new ASMItem(Reg::S0), new ASMItem(Reg::A1)}));
    } else {
      moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A2);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    }
    break;
  case Symbol::PARAM:
    if (ir->items[0]->symbol->dataType == Symbol::FLOAT) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::VSTR, {new ASMItem(Reg::S0), new ASMItem(Reg::A1)}));
    } else {
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A2);
      asms.push_back(
          new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
    }
    break;
  default:
    break;
  }
}

void ASMParser::parseMovSymbolReturnTempsOffset(
    vector<ASM *> &asms, IR *ir, vector<pair<unsigned, unsigned>> &temps,
    unsigned offset) {
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (ir->items[0]->symbol->dataType == Symbol::FLOAT) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A1);
      storeFromReg(asms, Reg::S0, Reg::A1, offset);
    } else {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A2),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A2),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A2);
      storeFromReg(asms, Reg::A1, Reg::A2, offset);
    }
    break;
  case Symbol::LOCAL_VAR:
    if (ir->items[0]->symbol->dataType == Symbol::FLOAT) {
      moveFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      storeFromReg(asms, Reg::S0, Reg::A1, offset);
    } else {
      moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A2);
      storeFromReg(asms, Reg::A1, Reg::A2, offset);
    }
    break;
  case Symbol::PARAM:
    if (ir->items[0]->symbol->dataType == Symbol::FLOAT) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      storeFromReg(asms, Reg::S0, Reg::A1, offset);
    } else {
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
      addTempsToReg(asms, temps, Reg::A2);
      storeFromReg(asms, Reg::A1, Reg::A2, offset);
    }
    break;
  default:
    break;
  }
}
