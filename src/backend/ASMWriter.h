#ifndef __ASM_WRITER_H__
#define __ASM_WRITER_H__

#include <string>
#include <utility>
#include <vector>

#include "../frontend/Symbol.h"
#include "ASM.h"

class ASMWriter {
private:
  bool isProcessed;
  std::string asmFile;
  std::vector<Symbol *> consts;
  std::vector<Symbol *> globalVars;
  std::unordered_map<Symbol *, std::vector<ASM *>> funcASMs;

public:
  ASMWriter(const std::string &, const std::vector<Symbol *> &,
            const std::vector<Symbol *> &,
            const std::unordered_map<Symbol *, std::vector<ASM *>> &);
  ~ASMWriter();

  void write();
};

#endif