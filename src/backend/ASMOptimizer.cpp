#include "ASMOptimizer.h"
#include "../GlobalData.h"

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
      switch (asms[i]->type) {
      case ASM::B:
        switch (asms[i + 1]->type) {
        case ASM::LABEL:
          if (asms[i]->items[0]->iVal == asms[i + 1]->items[0]->iVal)
            continue;
        default:
          newASMs.push_back(asms[i]);
          break;
        }
        break;
      default:
        newASMs.push_back(asms[i]);
        break;
      }
    }
    while (!newASMs.empty() && newASMs.back()->type == ASM::NOP)
      newASMs.pop_back();
    asms = newASMs;
  }
}