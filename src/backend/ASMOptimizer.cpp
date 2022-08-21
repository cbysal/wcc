#include "ASMOptimizer.h"
#include "../GlobalData.h"
#include <algorithm>
#include <iostream>
#include <queue>

using namespace std;

const std::vector<Reg::Type> aRegs = {
    Reg::A1,  Reg::A2,  Reg::A3,  Reg::A4,  Reg::S0,  Reg::S1, Reg::S2,
    Reg::S3,  Reg::S4,  Reg::S5,  Reg::S6,  Reg::S7,  Reg::S8, Reg::S9,
    Reg::S10, Reg::S11, Reg::S12, Reg::S13, Reg::S14, Reg::S15};

void ASMOptimizer::optimize() { peepholeOptimize(); }

void ASMOptimizer::peepholeOptimize() {
  for (unordered_map<Symbol *, vector<ASM *>>::iterator it = funcASMs.begin();
       it != funcASMs.end(); it++) {
    vector<ASM *> &asms = it->second;
    asms.push_back(new ASM(ASM::NOP, {}));
    asms.push_back(new ASM(ASM::NOP, {}));
    asms.push_back(new ASM(ASM::NOP, {}));
    vector<ASM *> newASMs;
    for (unsigned i = 0; i < asms.size() - 3; i++) {
      if (asms[i]->type == ASM::B && asms[i + 1]->type == ASM::LABEL)
        if (asms[i]->items[0]->iVal == asms[i + 1]->items[0]->iVal)
          continue;
      if (asms[i]->type == ASM::STR && asms[i + 1]->type == ASM::LDR) {
        Reg::Type reg1 = asms[i]->items[1]->reg;
        Reg::Type reg2 = asms[i + 1]->items[1]->reg;
        unsigned offset1 = 0;
        unsigned offset2 = 0;
        if (asms[i]->items.size() == 3)
          offset1 = asms[i]->items[2]->iVal;
        if (asms[i + 1]->items.size() == 3)
          offset2 = asms[i + 1]->items[2]->iVal;
        if (reg1 == reg2 && offset1 == offset2) {
          asms[i + 1]->type = ASM::MOV;
          asms[i + 1]->items.resize(2);
          asms[i + 1]->items[1]->type = ASMItem::REG;
          asms[i + 1]->items[1]->reg = asms[i]->items[0]->reg;
        }
      }
      if (asms[i]->type == ASM::MOV && asms[i + 1]->type == ASM::MOV) {
        if (asms[i]->items[0]->type == ASMItem::REG &&
            asms[i]->items[1]->type == ASMItem::INT &&
            asms[i + 1]->items[0]->type == ASMItem::REG &&
            asms[i + 1]->items[1]->type == ASMItem::REG &&
            asms[i]->items[0]->reg == asms[i + 1]->items[1]->reg) {
          asms[i + 1]->items[1]->type = ASMItem::INT;
          asms[i + 1]->items[1]->iVal = asms[i]->items[1]->iVal;
        }
        if (asms[i]->items[0]->type == ASMItem::REG &&
            asms[i]->items[1]->type == ASMItem::REG &&
            asms[i + 1]->items[0]->type == ASMItem::REG &&
            asms[i + 1]->items[1]->type == ASMItem::REG &&
            asms[i]->items[0]->reg == asms[i + 1]->items[1]->reg &&
            asms[i]->items[1]->reg == asms[i + 1]->items[0]->reg) {
          newASMs.push_back(asms[i]);
          i++;
          continue;
        }
      }
      if ((asms[i]->type == ASM::ADD || asms[i]->type == ASM::SUB) &&
          asms[i]->items[2]->type == ASMItem::INT && !asms[i]->items[2]->iVal &&
          asms[i]->items.size() == 3) {
        if (asms[i]->items[0]->reg == asms[i + 1]->items[0]->reg) {
          bool flag = false;
          for (unsigned j = 1; j < asms[i + 1]->items.size(); j++) {
            if (asms[i + 1]->items[j]->type == ASMItem::REG &&
                asms[i]->items[0]->reg == asms[i + 1]->items[j]->reg) {
              asms[i + 1]->items[j]->reg = asms[i]->items[1]->reg;
              flag = true;
            }
          }
          if (flag) {
            newASMs.push_back(asms[i + 1]);
            i++;
            continue;
          }
        }
        newASMs.push_back(
            new ASM(ASM::MOV, {asms[i]->items[0], asms[i]->items[1]}));
        continue;
      }
      if (asms[i]->type == ASM::ADD && asms[i + 1]->type == ASM::LDR &&
          asms[i + 1]->items.size() == 2) {
        if (asms[i]->items[0]->reg == asms[i + 1]->items[0]->reg &&
            asms[i]->items[0]->reg == asms[i + 1]->items[1]->reg) {
          asms[i + 1]->items.pop_back();
          asms[i + 1]->items.insert(asms[i + 1]->items.end(),
                                    asms[i]->items.begin() + 1,
                                    asms[i]->items.end());
          newASMs.push_back(asms[i + 1]);
          i++;
          continue;
        }
      }
      if (asms[i]->type == ASM::MVN && asms[i + 1]->type == ASM::MOV) {
        if (asms[i]->items[0]->type == ASMItem::REG &&
            asms[i]->items[1]->type == ASMItem::INT &&
            asms[i + 1]->items[0]->type == ASMItem::REG &&
            asms[i + 1]->items[1]->type == ASMItem::REG &&
            asms[i]->items[0]->reg == asms[i + 1]->items[1]->reg) {
          asms[i + 1]->type = ASM::MVN;
          asms[i + 1]->items[1]->type = ASMItem::INT;
          asms[i + 1]->items[1]->iVal = asms[i]->items[1]->iVal;
        }
      }
      newASMs.push_back(asms[i]);
    }
    while (!newASMs.empty() && newASMs.back()->type == ASM::NOP)
      newASMs.pop_back();
    asms = newASMs;
  }
}
