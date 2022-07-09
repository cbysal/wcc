#ifndef __ASM_WRITER_H__
#define __ASM_WRITER_H__

#include <utility>
#include <vector>

#include "../frontend/Symbol.h"
#include "ASM.h"

class ASMWriter {
private:
  bool isProcessed;
  string asmFile;
  vector<Symbol *> consts;
  vector<Symbol *> globalVars;
  vector<pair<Symbol *, vector<ASM *>>> funcASMs;

public:
  ASMWriter(const string &, const vector<Symbol *> &, const vector<Symbol *> &,
            const vector<pair<Symbol *, vector<ASM *>>> &);
  ~ASMWriter();

  void write();
};

#endif