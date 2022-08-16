#include <algorithm>

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

void ASMParser::makeRegFromTemps(vector<ASM *> &asms,
                                 vector<pair<unsigned, unsigned>> &temps,
                                 Reg::Type target) {
  bool first = true;
  for (pair<unsigned, unsigned> temp : temps) {
    bool flag = itemp2Reg.find(temp.first) == itemp2Reg.end();
    if (flag)
      loadFromSP(asms, Reg::A3, spillOffsets[temp.first]);
    if (first) {
      first = false;
      if (flag)
        loadFromSP(asms, target, spillOffsets[temp.first]);
      if (num2powerMap.find(temp.second) == num2powerMap.end()) {
        loadImmToReg(asms, Reg::A4, temp.second);
        asms.push_back(new ASM(
            ASM::MUL, {new ASMItem(target),
                       new ASMItem(flag ? Reg::A3 : itemp2Reg[temp.first]),
                       new ASMItem(Reg::A4)}));
      } else
        asms.push_back(new ASM(
            ASM::LSL, {new ASMItem(target),
                       new ASMItem(flag ? Reg::A3 : itemp2Reg[temp.first]),
                       new ASMItem(num2powerMap[temp.second])}));
    } else {
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
  vector<unsigned> sizes({4});
  for (int i = ir->items[1]->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * ir->items[1]->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
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
  if (ir->items[1]->symbol->dimensions.empty()) {
    if (flag1) {
      switch (ir->items[1]->symbol->symbolType) {
      case Symbol::CONST:
      case Symbol::GLOBAL_VAR:
        asms.push_back(
            new ASM(ASM::MOVW,
                    {new ASMItem(Reg::A1),
                     new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::MOVT,
                    {new ASMItem(Reg::A1),
                     new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
        break;
      case Symbol::LOCAL_VAR:
      case Symbol::PARAM:
        loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
        break;
      default:
        break;
      }
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else {
      switch (ir->items[1]->symbol->symbolType) {
      case Symbol::CONST:
      case Symbol::GLOBAL_VAR:
        asms.push_back(
            new ASM(ASM::MOVW,
                    {new ASMItem(Reg::A1),
                     new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::MOVT,
                    {new ASMItem(Reg::A1),
                     new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::VLDR, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                                new ASMItem(Reg::A1)}));
        break;
      case Symbol::LOCAL_VAR:
      case Symbol::PARAM:
        loadFromSP(asms, ftemp2Reg[ir->items[0]->iVal],
                   offsets[ir->items[1]->symbol]);
        break;
      default:
        break;
      }
    }
  } else {
    switch (ir->items[1]->symbol->symbolType) {
    case Symbol::CONST:
    case Symbol::GLOBAL_VAR:
      if (flag1) {
        asms.push_back(
            new ASM(ASM::MOVW,
                    {new ASMItem(Reg::A1),
                     new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::MOVT,
                    {new ASMItem(Reg::A1),
                     new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
        storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
      } else {
        asms.push_back(
            new ASM(ASM::MOVW,
                    {new ASMItem(Reg::A1),
                     new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::MOVT,
                    {new ASMItem(Reg::A1),
                     new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::VLDR, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                                new ASMItem(Reg::A1)}));
      }
      break;
    case Symbol::LOCAL_VAR:
      if (flag1) {
        loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
        asms.push_back(
            new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
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
}

void ASMParser::parseMovFtempSymbolOffset(vector<ASM *> &asms, IR *ir,
                                          unsigned offset) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end();
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    if (flag1) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      loadFromReg(asms, ftemp2Reg[ir->items[0]->iVal], Reg::A1, offset);
    }
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
    if (flag1) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      loadFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      loadFromReg(asms, ftemp2Reg[ir->items[0]->iVal], Reg::A1, offset);
    }
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
    if (flag1) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      makeRegFromTemps(asms, temps, Reg::A2);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                             new ASMItem(Reg::A2)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                              new ASMItem(Reg::A1)}));
    }
    break;
  case Symbol::LOCAL_VAR:
    if (flag1) {
      moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      makeRegFromTemps(asms, temps, Reg::A2);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                             new ASMItem(Reg::A2)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else {
      moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                              new ASMItem(Reg::A1)}));
    }
    break;
  case Symbol::PARAM:
    if (flag1) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      makeRegFromTemps(asms, temps, Reg::A2);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                             new ASMItem(Reg::A2)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::VLDR, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                              new ASMItem(Reg::A1)}));
    }
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
    if (flag1) {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      makeRegFromTemps(asms, temps, Reg::A2);
      moveFromReg(asms, Reg::A2, Reg::A2, offset);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                             new ASMItem(Reg::A2)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else {
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      addTempsToReg(asms, temps, Reg::A1);
      loadFromReg(asms, ftemp2Reg[ir->items[0]->iVal], Reg::A1, offset);
    }
    break;
  case Symbol::LOCAL_VAR:
    if (flag1) {
      moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      makeRegFromTemps(asms, temps, Reg::A2);
      moveFromReg(asms, Reg::A2, Reg::A2, offset);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                             new ASMItem(Reg::A2)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else {
      moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      loadFromReg(asms, ftemp2Reg[ir->items[0]->iVal], Reg::A1, offset);
    }
    break;
  case Symbol::PARAM:
    if (flag1) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      makeRegFromTemps(asms, temps, Reg::A2);
      moveFromReg(asms, Reg::A2, Reg::A2, offset);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                             new ASMItem(Reg::A2)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      loadFromReg(asms, ftemp2Reg[ir->items[0]->iVal], Reg::A1, offset);
    }
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
  vector<unsigned> sizes({4});
  for (int i = ir->items[1]->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * ir->items[1]->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
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
  if (ir->items[1]->symbol->dimensions.empty()) {
    switch (ir->items[1]->symbol->symbolType) {
    case Symbol::CONST:
    case Symbol::GLOBAL_VAR:
      asms.push_back(
          new ASM(ASM::MOVW,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(
          new ASM(ASM::MOVT,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::LDR,
          {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal])}));
      break;
    case Symbol::LOCAL_VAR:
    case Symbol::PARAM:
      loadFromSP(asms, flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      break;
    default:
      break;
    }
    if (flag1)
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
  } else {
    bool isFull =
        ir->items[1]->symbol->dimensions.size() + 2 == ir->items.size();
    switch (ir->items[1]->symbol->symbolType) {
    case Symbol::CONST:
    case Symbol::GLOBAL_VAR:
      if (flag1 && isFull) {
        asms.push_back(
            new ASM(ASM::MOVW,
                    {new ASMItem(Reg::A1),
                     new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::MOVT,
                    {new ASMItem(Reg::A1),
                     new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
        storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
      } else if (flag1 && !isFull) {
        asms.push_back(
            new ASM(ASM::MOVW,
                    {new ASMItem(Reg::A1),
                     new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::MOVT,
                    {new ASMItem(Reg::A1),
                     new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
        storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
      } else if (!flag1 && isFull) {
        asms.push_back(
            new ASM(ASM::MOVW,
                    {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                     new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::MOVT,
                    {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                     new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::LDR, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                               new ASMItem(itemp2Reg[ir->items[0]->iVal])}));
      } else if (!flag1 && !isFull) {
        asms.push_back(
            new ASM(ASM::MOVW,
                    {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                     new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
        asms.push_back(
            new ASM(ASM::MOVT,
                    {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                     new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      }
      break;
    case Symbol::LOCAL_VAR:
      if (flag1 && isFull) {
        loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
        asms.push_back(
            new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1)}));
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
      } else if (flag1 && !isFull &&
                 !ir->items[1]->symbol->dimensions.empty()) {
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
      makeRegFromTemps(asms, temps, Reg::A2);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                             new ASMItem(Reg::A2)}));
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
      makeRegFromTemps(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(Reg::A1)}));
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
      makeRegFromTemps(asms, temps, Reg::A2);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                             new ASMItem(Reg::A2)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull) {
      moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull) {
      moveFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      makeRegFromTemps(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(Reg::A1)}));
    } else if (!flag1 && !isFull) {
      moveFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      makeRegFromTemps(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(Reg::A1)}));
    }
    break;
  case Symbol::PARAM:
    if (flag1 && isFull) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      makeRegFromTemps(asms, temps, Reg::A2);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                             new ASMItem(Reg::A2)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull) {
      loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      makeRegFromTemps(asms, temps, Reg::A1);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(Reg::A1)}));
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
      makeRegFromTemps(asms, temps, Reg::A2);
      moveFromReg(asms, Reg::A2, Reg::A2, offset);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                             new ASMItem(Reg::A2)}));
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
      makeRegFromTemps(asms, temps, Reg::A1);
      moveFromReg(asms, Reg::A1, Reg::A1, offset);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(Reg::A1)}));
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
      makeRegFromTemps(asms, temps, Reg::A2);
      moveFromReg(asms, Reg::A2, Reg::A2, offset);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                             new ASMItem(Reg::A2)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull) {
      moveFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      moveFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull) {
      moveFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      makeRegFromTemps(asms, temps, Reg::A1);
      moveFromReg(asms, Reg::A1, Reg::A1, offset);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(Reg::A1)}));
    } else if (!flag1 && !isFull) {
      moveFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      makeRegFromTemps(asms, temps, Reg::A1);
      moveFromReg(asms, Reg::A1, Reg::A1, offset);
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(Reg::A1)}));
    }
    break;
  case Symbol::PARAM:
    if (flag1 && isFull) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      makeRegFromTemps(asms, temps, Reg::A2);
      moveFromReg(asms, Reg::A2, Reg::A2, offset);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                             new ASMItem(Reg::A2)}));
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !isFull) {
      loadFromSP(asms, Reg::A1, offsets[ir->items[1]->symbol]);
      addTempsToReg(asms, temps, Reg::A1);
      moveFromReg(asms, Reg::A1, Reg::A1, offset);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (!flag1 && isFull) {
      loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      makeRegFromTemps(asms, temps, Reg::A1);
      moveFromReg(asms, Reg::A1, Reg::A1, offset);
      asms.push_back(
          new ASM(ASM::LDR, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                             new ASMItem(Reg::A1)}));
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
  vector<unsigned> sizes({4});
  for (int i = ir->items[1]->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * ir->items[1]->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A2, offsets[ir->items[1]->symbol]);
    break;
  case Symbol::PARAM:
    if (ir->items[1]->symbol->dimensions.empty())
      moveFromSP(asms, Reg::A2, offsets[ir->items[1]->symbol]);
    else
      loadFromSP(asms, Reg::A2, offsets[ir->items[1]->symbol]);
    break;
  default:
    break;
  }
  for (pair<unsigned, unsigned> temp : temps) {
    if (itemp2Reg.find(temp.first) == itemp2Reg.end()) {
      loadFromSP(asms, Reg::A3, spillOffsets[temp.first]);
      loadImmToReg(asms, Reg::A4, temp.second);
      asms.push_back(
          new ASM(ASM::MUL, {new ASMItem(Reg::A3), new ASMItem(Reg::A3),
                             new ASMItem(Reg::A4)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                             new ASMItem(Reg::A3)}));
    } else {
      loadImmToReg(asms, Reg::A3, temp.second);
      asms.push_back(new ASM(ASM::MUL, {new ASMItem(Reg::A3),
                                        new ASMItem(itemp2Reg[temp.first]),
                                        new ASMItem(Reg::A3)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                             new ASMItem(Reg::A3)}));
    }
  }
  if (offset) {
    loadImmToReg(asms, Reg::A3, offset);
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                           new ASMItem(Reg::A3)}));
  }
  if (ir->items[1]->symbol->dataType == Symbol::INT)
    asms.push_back(
        new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
  else
    asms.push_back(
        new ASM(ASM::VLDR, {new ASMItem(Reg::S0), new ASMItem(Reg::A2)}));
}

void ASMParser::parseMovSymbolFtemp(vector<ASM *> &asms, IR *ir) {
  if (ir->items[0]->symbol->dimensions.empty()) {
    bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
    switch (ir->items[0]->symbol->symbolType) {
    case Symbol::CONST:
    case Symbol::GLOBAL_VAR:
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      asms.push_back(
          new ASM(ASM::STR,
                  {new ASMItem(flag2 ? Reg::A2 : ftemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A1)}));
      break;
    case Symbol::LOCAL_VAR:
    case Symbol::PARAM:
      storeFromSP(asms, flag2 ? Reg::A2 : ftemp2Reg[ir->items[1]->iVal],
                  offsets[ir->items[0]->symbol]);
      break;
    default:
      break;
    }
    return;
  }
  vector<unsigned> sizes({4});
  for (int i = ir->items[0]->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * ir->items[0]->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  case Symbol::PARAM:
    if (ir->items[0]->symbol->dimensions.empty())
      moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    else
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  default:
    break;
  }
  for (pair<unsigned, unsigned> temp : temps) {
    bool flag3 = itemp2Reg.find(temp.first) == itemp2Reg.end();
    if (flag3)
      loadFromSP(asms, Reg::A3, spillOffsets[temp.first]);
    if (num2powerMap.find(temp.second) == num2powerMap.end()) {
      loadImmToReg(asms, Reg::A4, temp.second);
      asms.push_back(new ASM(
          ASM::MLA, {new ASMItem(Reg::A2),
                     new ASMItem(flag3 ? Reg::A3 : itemp2Reg[temp.first]),
                     new ASMItem(Reg::A4), new ASMItem(Reg::A2)}));
    } else
      asms.push_back(new ASM(
          ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                     new ASMItem(flag3 ? Reg::A3 : itemp2Reg[temp.first]),
                     new ASMItem(ASMItem::LSL, num2powerMap[temp.second])}));
  }
  if (offset) {
    loadImmToReg(asms, Reg::A3, offset);
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                           new ASMItem(Reg::A3)}));
  }
  if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end()) {
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[1]->iVal]);
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(Reg::A3), new ASMItem(Reg::A2)}));
  } else
    asms.push_back(
        new ASM(ASM::VSTR, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                            new ASMItem(Reg::A2)}));
}

void ASMParser::parseMovSymbolImm(vector<ASM *> &asms, IR *ir) {
  vector<unsigned> sizes({4});
  for (int i = ir->items[0]->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * ir->items[0]->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  case Symbol::PARAM:
    if (ir->items[0]->symbol->dimensions.empty())
      moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    else
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  default:
    break;
  }
  for (pair<unsigned, unsigned> temp : temps) {
    if (itemp2Reg.find(temp.first) == itemp2Reg.end()) {
      loadFromSP(asms, Reg::A3, spillOffsets[temp.first]);
      loadImmToReg(asms, Reg::A4, temp.second);
      asms.push_back(
          new ASM(ASM::MUL, {new ASMItem(Reg::A3), new ASMItem(Reg::A3),
                             new ASMItem(Reg::A4)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                             new ASMItem(Reg::A3)}));
    } else {
      loadImmToReg(asms, Reg::A3, temp.second);
      asms.push_back(new ASM(ASM::MUL, {new ASMItem(Reg::A3),
                                        new ASMItem(itemp2Reg[temp.first]),
                                        new ASMItem(Reg::A3)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                             new ASMItem(Reg::A3)}));
    }
  }
  if (offset) {
    loadImmToReg(asms, Reg::A3, offset);
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                           new ASMItem(Reg::A3)}));
  }
  loadImmToReg(asms, Reg::A3, (unsigned)ir->items[1]->iVal);
  asms.push_back(
      new ASM(ASM::STR, {new ASMItem(Reg::A3), new ASMItem(Reg::A2)}));
}

void ASMParser::parseMovSymbolItemp(vector<ASM *> &asms, IR *ir) {
  bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (ir->items[0]->symbol->dimensions.empty()) {
    if (flag2)
      loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
    switch (ir->items[0]->symbol->symbolType) {
    case Symbol::CONST:
    case Symbol::GLOBAL_VAR:
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      asms.push_back(
          new ASM(ASM::STR,
                  {new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A1)}));
      break;
    case Symbol::LOCAL_VAR:
    case Symbol::PARAM:
      storeFromSP(asms, flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal],
                  offsets[ir->items[0]->symbol]);
      break;
    default:
      break;
    }
    return;
  }
  vector<unsigned> sizes({4});
  for (int i = ir->items[0]->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * ir->items[0]->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  case Symbol::PARAM:
    if (ir->items[0]->symbol->dimensions.empty())
      moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    else
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  default:
    break;
  }
  for (pair<unsigned, unsigned> temp : temps) {
    bool flag3 = itemp2Reg.find(temp.first) == itemp2Reg.end();
    if (flag3)
      loadFromSP(asms, Reg::A3, spillOffsets[temp.first]);
    if (num2powerMap.find(temp.second) == num2powerMap.end()) {
      loadImmToReg(asms, Reg::A4, temp.second);
      asms.push_back(new ASM(
          ASM::MLA, {new ASMItem(Reg::A2),
                     new ASMItem(flag3 ? Reg::A3 : itemp2Reg[temp.first]),
                     new ASMItem(Reg::A4), new ASMItem(Reg::A2)}));
    } else
      asms.push_back(new ASM(
          ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                     new ASMItem(flag3 ? Reg::A3 : itemp2Reg[temp.first]),
                     new ASMItem(ASMItem::LSL, num2powerMap[temp.second])}));
  }
  if (offset) {
    loadImmToReg(asms, Reg::A3, offset);
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                           new ASMItem(Reg::A3)}));
  }
  if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end()) {
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[1]->iVal]);
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(Reg::A3), new ASMItem(Reg::A2)}));
  } else
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                           new ASMItem(Reg::A2)}));
}

void ASMParser::parseMovSymbolReturn(vector<ASM *> &asms, IR *ir) {
  vector<unsigned> sizes({4});
  for (int i = ir->items[0]->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * ir->items[0]->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  case Symbol::PARAM:
    if (ir->items[0]->symbol->dimensions.empty())
      moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    else
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  default:
    break;
  }
  for (pair<unsigned, unsigned> temp : temps) {
    if (itemp2Reg.find(temp.first) == itemp2Reg.end()) {
      loadFromSP(asms, Reg::A3, spillOffsets[temp.first]);
      loadImmToReg(asms, Reg::A4, temp.second);
      asms.push_back(
          new ASM(ASM::MUL, {new ASMItem(Reg::A3), new ASMItem(Reg::A3),
                             new ASMItem(Reg::A4)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                             new ASMItem(Reg::A3)}));
    } else {
      loadImmToReg(asms, Reg::A3, temp.second);
      asms.push_back(new ASM(ASM::MUL, {new ASMItem(Reg::A3),
                                        new ASMItem(itemp2Reg[temp.first]),
                                        new ASMItem(Reg::A3)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                             new ASMItem(Reg::A3)}));
    }
  }
  if (offset) {
    loadImmToReg(asms, Reg::A3, offset);
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                           new ASMItem(Reg::A3)}));
  }
  if (ir->items[0]->symbol->dataType == Symbol::FLOAT)
    asms.push_back(
        new ASM(ASM::VSTR, {new ASMItem(Reg::S0), new ASMItem(Reg::A2)}));
  else
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
}
